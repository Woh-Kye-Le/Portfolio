-- =============================================================================
-- enemy_boss.lua  —  Final Boss (FINALBOSS mob type)
--
-- Mirrors C++ EnemyAi::updateBoss exactly.
--
-- Ability sequence  (BossSequencePhase):
--   MELEE        Chase player then strike; picks next ability after recovery.
--   RANGED_COMBO 3 normal shots then 3 spread bursts (same timing as ranged
--                enemy); picks next ability after end-of-combo pause.
--   BLESS_HEAL   Channel-cast to heal the nearest ally; skipped (rerolled)
--                when no allies are present.
--   BURROW       Telegraph → underground → emerge on player → stun window →
--                cooldown; picks next ability after cooldown.
--
-- Weighted random ability picker based on player distance (close/mid/far).
-- Never repeats back-to-back; BLESS_HEAL zeroed when no allies present.
--
-- Ally registration:
--   Call  RegisterBossAlly(ax, ay)  every frame for each living ally BEFORE
--   OnUpdate runs.  The table is consumed then cleared each update, matching
--   the C++ bossAllyPositions pattern.
--
-- Messages sent (same strings as C++):
--   "BossMeleeAttack"    — melee strike landed
--   "RangedNormalAttack" — single projectile toward player
--   "RangedSpreadAttack" — 3-projectile fan
--   "BossHealAlly"       — heal nearest ally (BLESS_HEAL)
--   "BurrowWarning"      — telegraph started (BURROW)
--   "BurrowUnderground"  — boss went underground
--   "BurrowStrike"       — boss emerged on player (hit)
--   "BurrowMissed"       — boss emerged, player dodged
--   "BurrowVulnerable"   — stun window opened
--   "BurrowInvulnerable" — stun window closed
--   "BossSequenceAdv"    — ability phase changed
-- =============================================================================

-- ---------------------------------------------------------------------------
-- Tuning constants  (match ai.cpp updateBoss locals)
-- ---------------------------------------------------------------------------
BOSS_HP                 = 100
BOSS_DAMAGE             = 5
BOSS_MOVE_SPEED         = 130.0
BOSS_MELEE_RANGE        = 2.5    -- tiles  (world = * tileSize)
BOSS_MELEE_RECOVERY     = 1.0    -- seconds
BOSS_RANGED_NORMAL_GAP  = 2.0
BOSS_RANGED_SPREAD_GAP  = 0.4
BOSS_RANGED_END_PAUSE   = 1.5
BOSS_HEAL_CAST_TIME     = 1.2
BOSS_BURROW_TELEGRAPH   = 0.8
BOSS_BURROW_TRAVEL      = 0.6
BOSS_BURROW_STUN        = 1.2
BOSS_BURROW_COOLDOWN    = 2.0
BOSS_HIT_RADIUS         = 0.9    -- tiles
BOSS_HEAL_RANGE         = 5.0    -- tiles

PATROL_SPEED  = 96.0
VISION_TILES  = 5.0              -- boss has wider aggro radius

-- ---------------------------------------------------------------------------
-- Sequence phase names  (index 0-3 maps to BossSequencePhase enum)
-- ---------------------------------------------------------------------------
SEQ_MELEE       = 0
SEQ_RANGED      = 1
SEQ_BLESS_HEAL  = 2
SEQ_BURROW      = 3

-- Burrow sub-phases
BURROW_ABOVE        = "ABOVE"
BURROW_TELEGRAPHING = "TELEGRAPHING"
BURROW_BURROWED     = "BURROWED"
BURROW_STRIKING     = "STRIKING"
BURROW_COOLDOWN     = "COOLDOWN"

-- FSM outer states
ST_PATROL = "PATROL"
ST_CHASE  = "CHASE"
ST_BOSS   = "BOSS"        -- active ability execution
ST_SEARCH = "SEARCH"
ST_RETURN = "RETURN"

-- ---------------------------------------------------------------------------
-- dt normalisation  (identical to every other enemy script)
-- ---------------------------------------------------------------------------
function normDt(dt)
    dt = tonumber(dt) or 0.0
    if dt > 1.0 then dt = dt * 0.001 end
    if dt > 0.05 then dt = 0.05 end
    return dt
end

-- ---------------------------------------------------------------------------
-- Engine helpers
-- ---------------------------------------------------------------------------
function getTileSize()
    local ts = 100.0
    if type(AI_TileSize) == "function" then
        local t = tonumber(AI_TileSize()) or 0.0
        if t > 0.0 then ts = t end
    end
    return ts
end

function isBlocked(wx, wy)
    if type(AI_IsBlocked) == "function" then return AI_IsBlocked(wx, wy) end
    return false
end

function boxBlocked(wx, wy)
    local ts = getTileSize(); local i = 2.0
    return isBlocked(wx+i, wy+i)       or isBlocked(wx+ts-i, wy+i)
        or isBlocked(wx+i, wy+ts-i)   or isBlocked(wx+ts-i, wy+ts-i)
end

function safeMove(e, vx, vy, dt)
    local ex, ey = GetPos(e)
    ex = tonumber(ex) or 0.0; ey = tonumber(ey) or 0.0
    local nx = ex + vx * dt
    if not boxBlocked(nx, ey) then ex = nx end
    local ny = ey + vy * dt
    if not boxBlocked(ex, ny) then ey = ny end
    SetPos(e, ex, ey)
end

function ai_dist(ax, ay, bx, by)
    local dx = bx-ax; local dy = by-ay
    return math.sqrt(dx*dx + dy*dy)
end

function ai_norm(dx, dy)
    local len = math.sqrt(dx*dx + dy*dy)
    if len < 0.001 then return 0, 0 end
    return dx/len, dy/len
end

-- Full LOS check (no cone restriction — boss detects from all directions)
function ai_hasLOS(ecx, ecy, px, py)
    if type(AI_HasLOS) == "function" then return AI_HasLOS(ecx, ecy, px, py) end
    return true
end

-- Cone + LOS check used for initial aggro (patrol → chase)
function ai_playerInFront(ecx, ecy, px, py, dirX, dirY)
    local ts  = getTileSize()
    local dx  = px - ecx; local dy = py - ecy
    local mag = ai_dist(ecx, ecy, px, py)
    if mag < 0.001 or mag > VISION_TILES * ts then return false end
    if (dx*dirX + dy*dirY) / mag < 0.7071 then return false end
    return ai_hasLOS(ecx, ecy, px, py)
end

function ai_distToPlayer(entity, px, py)
    local ts = getTileSize(); local ex, ey = GetPos(entity)
    return ai_dist(ex+ts*0.5, ey+ts*0.5, px+ts*0.25, py+ts*0.5)
end

-- ---------------------------------------------------------------------------
-- sendMsg — safe wrapper matching the guard used by other scripts
-- ---------------------------------------------------------------------------
function sendMsg(id)
    if type(SendMessage) == "function" then SendMessage(id) end
end

-- ---------------------------------------------------------------------------
-- Ally registration API — game system calls this each frame for living allies
-- ---------------------------------------------------------------------------
s_bossAllies = {}    -- {x, y} entries filled externally each frame

function RegisterBossAlly(ax, ay)
    ax = tonumber(ax) or 0.0; ay = tonumber(ay) or 0.0
    s_bossAllies[#s_bossAllies + 1] = { x = ax, y = ay }
end

-- ---------------------------------------------------------------------------
-- Patrol
-- ---------------------------------------------------------------------------
function ai_updatePatrol(entity, dt)
    local ts    = getTileSize(); local ex, ey = GetPos(entity)
    local dirX  = s_dirX or 1;  local dirY = s_dirY or 0
    local speed = PATROL_SPEED
    local nx = ex + dirX*speed*dt; local ny = ey + dirY*speed*dt
    local hitWall = boxBlocked(nx, ny)
    local sgx = math.floor((s_spawnX or ex) / ts)
    local sgy = math.floor((s_spawnY or ey) / ts)
    local ngx = math.floor(nx / ts); local ngy = math.floor(ny / ts)
    local pR  = s_patrolRange or 5
    local out = (dirX > 0 and ngx > sgx+pR) or (dirX < 0 and ngx < sgx-pR)
             or (dirY > 0 and ngy > sgy+pR) or (dirY < 0 and ngy < sgy-pR)
    if hitWall or out then
        s_dirX = -dirX; s_dirY = -dirY
    else
        safeMove(entity, dirX*speed, dirY*speed, dt)
    end
end

-- ---------------------------------------------------------------------------
-- Chase (direct toward player, axis-separated collision)
-- ---------------------------------------------------------------------------
function ai_updateChase(entity, dt, px, py)
    local ts = getTileSize(); local ex, ey = GetPos(entity)
    s_lastKnownPX = px; s_lastKnownPY = py
    local cx = ex+ts*0.5; local cy = ey+ts*0.5
    local pcx = px+ts*0.25; local pcy = py+ts*0.5
    local dx = pcx-cx; local dy = pcy-cy
    local dist = math.sqrt(dx*dx + dy*dy)
    if dist < 1e-3 then return end
    local dirX = dx/dist; local dirY = dy/dist
    local speed = BOSS_MOVE_SPEED
    if math.abs(dx) > math.abs(dy) then s_dirX = dx>0 and 1 or -1; s_dirY = 0
    else s_dirX = 0; s_dirY = dy>0 and 1 or -1 end
    local stepX = dirX*speed*dt; local stepY = dirY*speed*dt
    if boxBlocked(ex+stepX, ey+stepY) then
        if not boxBlocked(ex+stepX, ey) then safeMove(entity, dirX*speed, 0, dt)
        elseif not boxBlocked(ex, ey+stepY) then safeMove(entity, 0, dirY*speed, dt)
        end
    else
        safeMove(entity, dirX*speed, dirY*speed, dt)
    end
end

-- ---------------------------------------------------------------------------
-- Search (walk to last known position, then 360-degree scan)
-- ---------------------------------------------------------------------------
function ai_updateSearch(entity, dt)
    local ts = getTileSize(); local ex, ey = GetPos(entity)
    local geX = math.floor(ex/ts); local geY = math.floor(ey/ts)
    local gtX = math.floor((s_lastKnownPX or ex)/ts)
    local gtY = math.floor((s_lastKnownPY or ey)/ts)

    if s_hasLastSeen then
        if geX==gtX and geY==gtY then
            s_hasLastSeen=false; s_searchTimer=0; s_searchPhase=0; return
        end
        local ddx=gtX-geX; local ddy=gtY-geY
        local nX=geX; local nY=geY
        if math.abs(ddx)>=math.abs(ddy) then nX=geX+(ddx>0 and 1 or -1)
        else nY=geY+(ddy>0 and 1 or -1) end
        local wx=nX*ts+ts*0.5; local wy=nY*ts+ts*0.5
        if not isBlocked(wx,wy) then
            local ndx,ndy=ai_norm(wx-(ex+ts*0.5), wy-(ey+ts*0.5))
            safeMove(entity, ndx*PATROL_SPEED, ndy*PATROL_SPEED, dt)
        else
            local aX=geX; local aY=geY
            if math.abs(ddx)>=math.abs(ddy) then aY=geY+(ddy>0 and 1 or -1)
            else aX=geX+(ddx>0 and 1 or -1) end
            local ax=aX*ts+ts*0.5; local ay=aY*ts+ts*0.5
            if not isBlocked(ax,ay) then
                local ndx,ndy=ai_norm(ax-(ex+ts*0.5), ay-(ey+ts*0.5))
                safeMove(entity, ndx*PATROL_SPEED, ndy*PATROL_SPEED, dt)
            end
        end
        return
    end

    -- 360-degree scan (4 direction turns over ~2 s)
    s_searchTimer = (s_searchTimer or 0) + dt
    local dirs = {{1,0},{0,1},{-1,0},{0,-1}}
    local ph = s_searchPhase or 0
    if s_searchTimer > (ph+1)*0.5 then
        ph = ph+1; s_searchPhase = ph
        if ph >= 4 then s_searchDone = true end
    end
    if ph < 4 then s_dirX = dirs[ph+1][1]; s_dirY = dirs[ph+1][2] end
end

-- ---------------------------------------------------------------------------
-- Return to spawn
-- ---------------------------------------------------------------------------
function ai_moveToSpawn(entity, dt)
    local ts = getTileSize(); local ex, ey = GetPos(entity)
    local scx = (s_spawnX or ex)+ts*0.5; local scy = (s_spawnY or ey)+ts*0.5
    local ecx = ex+ts*0.5;               local ecy = ey+ts*0.5
    local dX = scx-ecx; local dY = scy-ecy
    s_atSpawn = false
    if dX*dX+dY*dY < 4.0 then
        s_atSpawn = true
        SetPos(entity, s_spawnX or ex, s_spawnY or ey); return
    end
    local dist = math.sqrt(dX*dX+dY*dY)
    local ndx, ndy = dX/dist, dY/dist
    local speed = PATROL_SPEED
    if math.abs(dX)>math.abs(dY) then s_dirX=dX>0 and 1 or -1; s_dirY=0
    else s_dirX=0; s_dirY=dY>0 and 1 or -1 end
    if boxBlocked(ex+ndx*speed*dt, ey+ndy*speed*dt) then
        if not boxBlocked(ex+ndx*speed*dt, ey) then safeMove(entity, ndx*speed, 0, dt)
        elseif not boxBlocked(ex, ey+ndy*speed*dt) then safeMove(entity, 0, ndy*speed, dt)
        end
    else
        safeMove(entity, ndx*speed, ndy*speed, dt)
    end
end

-- ===========================================================================
-- updateBoss — mirrors EnemyAi::updateBoss in ai.cpp
-- ===========================================================================

-- ---------------------------------------------------------------------------
-- pickNextAbility  (weighted random, no back-to-back, BLESS_HEAL gated)
-- ---------------------------------------------------------------------------
function pickNextAbility(dist, ts, alliesPresent)
    local meleeRange = BOSS_MELEE_RANGE * ts

    -- Weight tables: [MELEE, RANGED, BLESS_HEAL, BURROW]
    local w = {}
    if dist <= meleeRange * 1.5 then
        w = {70, 10, 10, 10}   -- close range
    elseif dist <= meleeRange * 3.5 then
        w = {30, 30, 20, 20}   -- mid range
    else
        w = {10, 40, 20, 30}   -- far range
    end

    -- No allies alive → zero BLESS_HEAL weight entirely
    if not alliesPresent then w[SEQ_BLESS_HEAL + 1] = 0 end

    -- Zero out last-used phase to prevent back-to-back repeats
    local lastIdx = (s_bossSeqPhase or SEQ_MELEE) + 1
    w[lastIdx] = 0

    local total = w[1] + w[2] + w[3] + w[4]
    if total <= 0 then return SEQ_MELEE end   -- absolute fallback

    local roll = math.random(0, total - 1)
    local cum  = 0
    for i = 1, 4 do
        cum = cum + w[i]
        if roll < cum then return i - 1 end
    end
    return SEQ_MELEE
end

-- ---------------------------------------------------------------------------
-- advanceTo  — clean transition to next ability phase
-- ---------------------------------------------------------------------------
function advanceTo(next)
    s_bossSeqPhase    = next
    s_bossAbilityTimer = 0.0
    s_bossAbilityDone  = false
    s_attackTimer      = 0.0
    if next == SEQ_BURROW then
        s_burrowPhase      = BURROW_ABOVE
        s_burrowPhaseTimer = 0.0
        s_isBurrowed       = false
    end
    if next == SEQ_RANGED then
        s_bossRangedShots = 0
    end
    sendMsg("BossSequenceAdv")
end

-- ---------------------------------------------------------------------------
-- updateBoss  — called each frame while s_fsm == ST_BOSS
-- ---------------------------------------------------------------------------
function updateBoss(entity, dt, px, py)
    local ts = getTileSize()
    local ex, ey = GetPos(entity)
    ex = tonumber(ex) or 0.0; ey = tonumber(ey) or 0.0

    local ecx = ex + ts*0.5;   local ecy = ey + ts*0.5
    local pcx = px + ts*0.25;  local pcy = py + ts*0.5
    local dx  = pcx - ecx;     local dy  = pcy - ecy
    local dist = math.sqrt(dx*dx + dy*dy)

    -- Snapshot and clear ally list (refilled by game system next frame)
    local allies = s_bossAllies
    s_bossAllies = {}
    local alliesPresent = (#allies > 0)

    -- Face the player
    if math.abs(dx) > math.abs(dy) then
        s_dirX = dx > 0 and 1 or -1; s_dirY = 0
    else
        s_dirX = 0; s_dirY = dy > 0 and 1 or -1
    end

    local meleeRange = BOSS_MELEE_RANGE * ts

    -- =========================================================================
    -- MELEE
    -- =========================================================================
    if s_bossSeqPhase == SEQ_MELEE then

        if dist > meleeRange then
            -- Close in on player
            local ndx, ndy = ai_norm(dx, dy)
            local vx = ndx * BOSS_MOVE_SPEED; local vy = ndy * BOSS_MOVE_SPEED
            safeMove(entity, vx, vy, dt)
            s_bossAbilityTimer = 0.0; s_bossAbilityDone = false

        elseif not s_bossAbilityDone then
            -- In melee range — strike
            sendMsg("BossMeleeAttack")
            s_bossAbilityDone = true; s_bossAbilityTimer = 0.0

        else
            -- Recovery pause then pick next ability
            s_bossAbilityTimer = s_bossAbilityTimer + dt
            if s_bossAbilityTimer >= BOSS_MELEE_RECOVERY then
                advanceTo(pickNextAbility(dist, ts, alliesPresent))
            end
        end

    -- =========================================================================
    -- RANGED_COMBO  (3 normal + 3 spread, mirrors C++ and enemy_ranged.lua)
    -- =========================================================================
    elseif s_bossSeqPhase == SEQ_RANGED then

        if s_bossAbilityDone then
            -- End-of-combo pause
            s_bossAbilityTimer = s_bossAbilityTimer + dt
            if s_bossAbilityTimer >= BOSS_RANGED_END_PAUSE then
                advanceTo(pickNextAbility(dist, ts, alliesPresent))
            end
            return
        end

        s_attackTimer = s_attackTimer - dt
        if s_attackTimer > 0.0 then return end

        if s_bossRangedShots < 3 then
            sendMsg("RangedNormalAttack")
            s_attackTimer = BOSS_RANGED_NORMAL_GAP
        else
            sendMsg("RangedSpreadAttack")
            s_attackTimer = BOSS_RANGED_SPREAD_GAP
        end

        s_bossRangedShots = s_bossRangedShots + 1
        if s_bossRangedShots >= 6 then
            s_bossAbilityDone  = true
            s_bossAbilityTimer = 0.0
        end

    -- =========================================================================
    -- BLESS_HEAL  (channel cast to heal nearest ally)
    -- =========================================================================
    elseif s_bossSeqPhase == SEQ_BLESS_HEAL then

        -- Allies are guaranteed present here: pickNextAbility zeroed the weight
        -- when alliesPresent == false, making this branch unreachable then.

        if not s_bossAbilityDone then
            s_bossAbilityTimer = s_bossAbilityTimer + dt
            if s_bossAbilityTimer >= BOSS_HEAL_CAST_TIME then
                sendMsg("BossHealAlly")
                s_bossAbilityDone  = true
                s_bossAbilityTimer = 0.0
            end
        else
            -- Short pause after heal, then pick next
            s_bossAbilityTimer = s_bossAbilityTimer + dt
            if s_bossAbilityTimer >= 0.5 then
                advanceTo(pickNextAbility(dist, ts, alliesPresent))
            end
        end

    -- =========================================================================
    -- BURROW  (mirrors C++ BURROW case inside updateBoss)
    -- =========================================================================
    elseif s_bossSeqPhase == SEQ_BURROW then

        local burrowRange = BOSS_MELEE_RANGE * ts  -- same threshold as C++

        -- ----------------------------------------------------------------
        -- ABOVE_GROUND: walk toward player; telegraph when in burrow range
        -- ----------------------------------------------------------------
        if s_burrowPhase == BURROW_ABOVE then

            if dist > burrowRange then
                local ndx, ndy = ai_norm(dx, dy)
                safeMove(entity, ndx*BOSS_MOVE_SPEED, ndy*BOSS_MOVE_SPEED, dt)
            else
                -- Lock strike target to player's current position
                s_burrowStrikeX   = pcx
                s_burrowStrikeY   = pcy
                s_burrowPhaseTimer = BOSS_BURROW_TELEGRAPH
                s_burrowPhase      = BURROW_TELEGRAPHING
                s_isBurrowed       = false
                sendMsg("BurrowWarning")
            end

        -- ----------------------------------------------------------------
        -- TELEGRAPHING: stand still, count down telegraph window
        -- ----------------------------------------------------------------
        elseif s_burrowPhase == BURROW_TELEGRAPHING then

            s_burrowPhaseTimer = s_burrowPhaseTimer - dt
            if s_burrowPhaseTimer <= 0.0 then
                s_burrowPhase      = BURROW_BURROWED
                s_burrowPhaseTimer = BOSS_BURROW_TRAVEL
                s_isBurrowed       = true
                sendMsg("BurrowUnderground")
            end

        -- ----------------------------------------------------------------
        -- BURROWED: travel underground, emerge at locked strike target
        -- ----------------------------------------------------------------
        elseif s_burrowPhase == BURROW_BURROWED then

            s_burrowPhaseTimer = s_burrowPhaseTimer - dt
            if s_burrowPhaseTimer <= 0.0 then
                -- Snap to strike target
                local emergeX = s_burrowStrikeX - ts*0.5
                local emergeY = s_burrowStrikeY - ts*0.5
                SetPos(entity, emergeX, emergeY)
                s_isBurrowed = false

                -- Re-measure distance at emerge point
                local nEcx = emergeX + ts*0.5
                local nEcy = emergeY + ts*0.5
                local eDist = ai_dist(nEcx, nEcy, pcx, pcy)
                local hitRadius = BOSS_HIT_RADIUS * ts

                if eDist <= hitRadius then
                    sendMsg("BurrowStrike")
                else
                    sendMsg("BurrowMissed")
                end
                sendMsg("BurrowVulnerable")

                s_burrowPhase      = BURROW_STRIKING
                s_burrowPhaseTimer = BOSS_BURROW_STUN
            end

        -- ----------------------------------------------------------------
        -- STRIKING: stun window — fully frozen, player's punish chance
        -- ----------------------------------------------------------------
        elseif s_burrowPhase == BURROW_STRIKING then

            s_burrowPhaseTimer = s_burrowPhaseTimer - dt
            if s_burrowPhaseTimer <= 0.0 then
                sendMsg("BurrowInvulnerable")
                s_burrowPhase      = BURROW_COOLDOWN
                s_burrowPhaseTimer = BOSS_BURROW_COOLDOWN
            end

        -- ----------------------------------------------------------------
        -- COOLDOWN: brief visible pause before picking next ability
        -- ----------------------------------------------------------------
        elseif s_burrowPhase == BURROW_COOLDOWN then

            s_burrowPhaseTimer = s_burrowPhaseTimer - dt
            if s_burrowPhaseTimer <= 0.0 then
                s_burrowPhase = BURROW_ABOVE
                s_isBurrowed  = false
                advanceTo(pickNextAbility(dist, ts, alliesPresent))
            end
        end

    end -- end switch bossSeqPhase
end

-- ===========================================================================
-- OnStart
-- ===========================================================================
function OnStart(entity)
    local ts = getTileSize(); local ex, ey = GetPos(entity)

    -- Outer FSM
    s_fsm         = ST_PATROL
    s_dirX        = 1; s_dirY = 0
    s_spawnX      = ex; s_spawnY = ey
    s_patrolRange = 5

    -- Search state
    s_lastKnownPX = ex; s_lastKnownPY = ey
    s_hasLastSeen = false; s_searchTimer = 0; s_searchPhase = 0; s_searchDone = false

    -- Return-to-spawn state
    s_atSpawn = false

    -- Boss ability state
    s_bossSeqPhase    = SEQ_MELEE     -- start with MELEE on first aggro
    s_bossAbilityTimer = 0.0
    s_bossAbilityDone  = false
    s_attackTimer      = 0.0
    s_bossRangedShots  = 0

    -- Burrow sub-state
    s_burrowPhase      = BURROW_ABOVE
    s_burrowPhaseTimer = 0.0
    s_isBurrowed       = false
    s_burrowStrikeX    = 0.0; s_burrowStrikeY = 0.0

    -- Ally list
    s_bossAllies = {}

    s_tileSize = ts
end

-- ===========================================================================
-- OnUpdate
-- ===========================================================================
function OnUpdate(entity, dt)
    dt = normDt(dt)
    if s_fsm == nil then OnStart(entity) end

    local player = FindPlayer(); if player == nil then return end
    local px, py = GetPos(player)
    px = tonumber(px) or 0.0; py = tonumber(py) or 0.0

    local ex, ey = GetPos(entity)
    ex = tonumber(ex) or 0.0; ey = tonumber(ey) or 0.0
    local ts  = s_tileSize or getTileSize()
    local ecx = ex + ts*0.5; local ecy = ey + ts*0.5

    -- =======================================================================
    -- PATROL — wander until player enters vision cone or LOS
    -- =======================================================================
    if s_fsm == ST_PATROL then
        if ai_playerInFront(ecx, ecy, px, py, s_dirX, s_dirY)
        or ai_hasLOS(ecx, ecy, px, py) then
            s_fsm = ST_CHASE; return
        end
        ai_updatePatrol(entity, dt)

    -- =======================================================================
    -- CHASE — close in until within melee range, then enter ability loop
    -- =======================================================================
    elseif s_fsm == ST_CHASE then
        if not ai_hasLOS(ecx, ecy, px, py) then
            s_fsm = ST_SEARCH; s_searchTimer = 0; s_searchPhase = 0
            s_searchDone = false; s_hasLastSeen = true; return
        end
        local distTile = ai_distToPlayer(entity, px, py)
        if distTile <= BOSS_MELEE_RANGE * ts then
            -- Enter ability loop; MELEE is the first move
            advanceTo(SEQ_MELEE)
            s_fsm = ST_BOSS; return
        end
        ai_updateChase(entity, dt, px, py)

    -- =======================================================================
    -- BOSS  — run weighted ability sequence
    -- =======================================================================
    elseif s_fsm == ST_BOSS then
        -- Re-engage chase if boss somehow gets far away (e.g. knock-back)
        if not ai_hasLOS(ecx, ecy, px, py) then
            s_fsm = ST_SEARCH; s_searchTimer = 0; s_searchPhase = 0
            s_searchDone = false; s_hasLastSeen = true; return
        end
        updateBoss(entity, dt, px, py)

    -- =======================================================================
    -- SEARCH — walk to last known position, scan, then return to spawn
    -- =======================================================================
    elseif s_fsm == ST_SEARCH then
        if ai_playerInFront(ecx, ecy, px, py, s_dirX, s_dirY)
        or ai_hasLOS(ecx, ecy, px, py) then
            s_fsm = ST_CHASE; return
        end
        ai_updateSearch(entity, dt)
        if s_searchDone then
            s_searchDone = false; s_fsm = ST_RETURN; s_atSpawn = false
        end

    -- =======================================================================
    -- RETURN — walk back to spawn; resume patrol on arrival
    -- =======================================================================
    elseif s_fsm == ST_RETURN then
        if ai_playerInFront(ecx, ecy, px, py, s_dirX, s_dirY)
        or ai_hasLOS(ecx, ecy, px, py) then
            s_fsm = ST_CHASE; return
        end
        ai_moveToSpawn(entity, dt)
        if s_atSpawn then s_fsm = ST_PATROL end
    end
end
