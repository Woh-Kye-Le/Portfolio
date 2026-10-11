--[[
* @file     enemy_burrow.lua
* @author   Lim Zhi Jie
* @email    zhijie.lim@digipen.edu
* @date     2025-03-11
*
* @brief Implements the Burrow enemy AI behavior (whack-a-mole underground attack).
*
* This script defines the state machine and attack sequence for the burrow
* enemy type. When the player enters burrow range, the enemy locks onto
* the player's position, telegraphs the attack, disappears underground for a
* travel period, then teleports via SetPos to emerge directly on the player.
* After the strike window expires, the enemy enters a cooldown phase during
* which it slowly retreats from the player before resurfacing. Movement
* during above-ground phases uses axis-separated collision via safeMove.
*
* Burrow phases: ABOVE_GROUND → TELEGRAPHING → BURROWED → STRIKING → COOLDOWN
* States: PATROL → CHASE → ATTACK → SEARCH → RETURN → PATROL
*
* @version 1.0
* @copyright Copyright (C) 2025 DigiPen Institute of Technology.
* Reproduction or disclosure of this file or its contents without the
* prior written consent of DigiPen Institute of Technology is prohibited.
*]]

-- =============================================================================
-- enemy_burrow.lua  —  Burrow enemy (whack-a-mole underground attack)
-- Movement uses SetPos directly (same pattern as chase.lua).
-- =============================================================================

BURROW_RANGE_TILES = 2.0
BURROW_SPEED       = 80.0
TELEGRAPH_DUR      = 0.8
BURROW_TRAVEL_TIME = 0.6
STRIKE_DUR         = 1.2
COOLDOWN_TIME      = 2.5
HIT_RADIUS_FACTOR  = 0.9
PATROL_SPEED       = 96.0
CHASE_SPEED        = 150.0
SEARCH_SPEED       = 60.0
VISION_TILES       = 3.0

ST_PATROL = "PATROL"
ST_CHASE  = "CHASE"
ST_SEARCH = "SEARCH"
ST_RETURN = "RETURN"
ST_ATTACK = "ATTACK"

BP_ABOVE  = "ABOVE_GROUND"
BP_TELE   = "TELEGRAPHING"
BP_BURIED = "BURROWED"
BP_STRIKE = "STRIKING"
BP_COOL   = "COOLDOWN"

-- ---------------------------------------------------------------------------
-- Shared helpers
-- ---------------------------------------------------------------------------
function normDt(dt)
    dt=tonumber(dt) or 0.0;if dt>1.0 then dt=dt*0.001 end;if dt>0.05 then dt=0.05 end;return dt
end
function getTileSize()
    local ts=100.0;if type(AI_TileSize)=="function" then local t=tonumber(AI_TileSize()) or 0.0;if t>0.0 then ts=t end end;return ts
end
function isBlocked(wx,wy) if type(AI_IsBlocked)=="function" then return AI_IsBlocked(wx,wy) end;return false end
function boxBlocked(wx,wy)
    local ts=getTileSize();local i=2.0
    return isBlocked(wx+i,wy+i) or isBlocked(wx+ts-i,wy+i) or isBlocked(wx+i,wy+ts-i) or isBlocked(wx+ts-i,wy+ts-i)
end
function safeMove(e,vx,vy,dt)
    local ex,ey=GetPos(e);ex=tonumber(ex) or 0.0;ey=tonumber(ey) or 0.0
    local nx=ex+vx*dt;if not boxBlocked(nx,ey) then ex=nx end
    local ny=ey+vy*dt;if not boxBlocked(ex,ny) then ey=ny end
    SetPos(e,ex,ey)
end
function ai_dist(ax,ay,bx,by) local dx=bx-ax;local dy=by-ay;return math.sqrt(dx*dx+dy*dy) end
function ai_norm(dx,dy) local l=math.sqrt(dx*dx+dy*dy);if l<0.001 then return 0,0 end;return dx/l,dy/l end
function ai_hasLOS(ax,ay,bx,by) if type(AI_HasLOS)=="function" then return AI_HasLOS(ax,ay,bx,by) end;return true end
function ai_playerInFront(ecx,ecy,px,py,dX,dY)
    local ts=getTileSize();local dx=px-ecx;local dy=py-ecy;local mag=ai_dist(ecx,ecy,px,py)
    if mag<0.001 or mag>VISION_TILES*ts then return false end
    if (dx*dX+dy*dY)/mag<0.7071 then return false end;return ai_hasLOS(ecx,ecy,px,py)
end
function ai_distToPlayer(entity,px,py)
    local ts=getTileSize();local ex,ey=GetPos(entity);return ai_dist(ex+ts*0.5,ey+ts*0.5,px+ts*0.25,py+ts*0.5)
end
function ai_updatePatrol(entity,dt)
    local ts=getTileSize();local ex,ey=GetPos(entity);local dX=s_dirX or 1;local dY=s_dirY or 0;local speed=PATROL_SPEED
    local nx=ex+dX*speed*dt;local ny=ey+dY*speed*dt
    local sgx=math.floor((s_spawnX or ex)/ts);local sgy=math.floor((s_spawnY or ey)/ts);local pR=s_patrolRange or 5
    local out=(dX>0 and math.floor(nx/ts)>sgx+pR)or(dX<0 and math.floor(nx/ts)<sgx-pR)
           or(dY>0 and math.floor(ny/ts)>sgy+pR)or(dY<0 and math.floor(ny/ts)<sgy-pR)
    if boxBlocked(nx,ny) or out then s_dirX=-dX;s_dirY=-dY else safeMove(entity,dX*speed,dY*speed,dt) end
end
function ai_updateChase(entity,dt,px,py)
    local ts=getTileSize();local ex,ey=GetPos(entity);s_lastKnownPX=px;s_lastKnownPY=py
    local cx=ex+ts*0.5;local cy=ey+ts*0.5;local dx=(px+ts*0.5)-cx;local dy=(py+ts*0.5)-cy
    local dist=math.sqrt(dx*dx+dy*dy);if dist<1e-3 then return end
    local dX=dx/dist;local dY=dy/dist;local speed=CHASE_SPEED
    if math.abs(dx)>math.abs(dy) then s_dirX=dx>0 and 1 or -1;s_dirY=0 else s_dirX=0;s_dirY=dy>0 and 1 or -1 end
    local sx=dX*speed*dt;local sy=dY*speed*dt
    if boxBlocked(ex+sx,ey+sy) then
        if not boxBlocked(ex+sx,ey) then safeMove(entity,dX*speed,0,dt)
        elseif not boxBlocked(ex,ey+sy) then safeMove(entity,0,dY*speed,dt) end
    else safeMove(entity,dX*speed,dY*speed,dt) end
end
function ai_updateSearch(entity,dt)
    local ts=getTileSize();local ex,ey=GetPos(entity)
    local geX=math.floor(ex/ts);local geY=math.floor(ey/ts)
    local gtX=math.floor((s_lastKnownPX or ex)/ts);local gtY=math.floor((s_lastKnownPY or ey)/ts)
    if s_hasLastSeen then
        if geX==gtX and geY==gtY then s_hasLastSeen=false;s_searchTimer=0;s_searchPhase=0;return end
        local ddx=gtX-geX;local ddy=gtY-geY;local nX=geX;local nY=geY
        if math.abs(ddx)>=math.abs(ddy) then nX=geX+(ddx>0 and 1 or -1) else nY=geY+(ddy>0 and 1 or -1) end
        if not isBlocked(nX*ts+ts*0.5,nY*ts+ts*0.5) then
            local nx2,ny2=ai_norm(nX*ts+ts*0.5-(ex+ts*0.5),nY*ts+ts*0.5-(ey+ts*0.5));safeMove(entity,nx2*SEARCH_SPEED,ny2*SEARCH_SPEED,dt)
        else
            local aX=geX;local aY=geY
            if math.abs(ddx)>=math.abs(ddy) then aY=geY+(ddy>0 and 1 or -1) else aX=geX+(ddx>0 and 1 or -1) end
            if not isBlocked(aX*ts+ts*0.5,aY*ts+ts*0.5) then
                local nx2,ny2=ai_norm(aX*ts+ts*0.5-(ex+ts*0.5),aY*ts+ts*0.5-(ey+ts*0.5));safeMove(entity,nx2*SEARCH_SPEED,ny2*SEARCH_SPEED,dt)
            end
        end
        return
    end
    s_searchTimer=(s_searchTimer or 0)+dt
    local dirs={{1,0},{0,1},{-1,0},{0,-1}};local ph=s_searchPhase or 0
    if s_searchTimer>(ph+1)*0.5 then ph=ph+1;s_searchPhase=ph;if ph>=4 then s_searchDone=true end end
    if ph<4 then s_dirX=dirs[ph+1][1];s_dirY=dirs[ph+1][2] end
end
function ai_moveToSpawn(entity,dt)
    local ts=getTileSize();local ex,ey=GetPos(entity)
    local ecx=ex+ts*0.5;local ecy=ey+ts*0.5
    local scx=(s_spawnX or ex)+ts*0.5;local scy=(s_spawnY or ey)+ts*0.5
    local dX=scx-ecx;local dY=scy-ecy;s_atSpawn=false
    if dX*dX+dY*dY<4.0 then s_atSpawn=true;s_movingToTile=false;SetPos(entity,s_spawnX or ex,s_spawnY or ey);return end
    local dist=math.sqrt(dX*dX+dY*dY);local ndx=dX/dist;local ndy=dY/dist;local speed=PATROL_SPEED
    if math.abs(dX)>math.abs(dY) then s_dirX=dX>0 and 1 or -1;s_dirY=0 else s_dirX=0;s_dirY=dY>0 and 1 or -1 end
    if boxBlocked(ex+ndx*speed*dt,ey+ndy*speed*dt) then
        if not boxBlocked(ex+ndx*speed*dt,ey) then safeMove(entity,ndx*speed,0,dt)
        elseif not boxBlocked(ex,ey+ndy*speed*dt) then safeMove(entity,0,ndy*speed,dt) end
    else safeMove(entity,ndx*speed,ndy*speed,dt) end
end

-- ===========================================================================
-- OnStart
-- ===========================================================================
function OnStart(entity)
    local ts=getTileSize();local ex,ey=GetPos(entity)
    s_fsm=ST_PATROL;s_dirX=1;s_dirY=0
    s_spawnX=ex;s_spawnY=ey;s_patrolRange=5
    s_lastKnownPX=ex;s_lastKnownPY=ey
    s_hasLastSeen=false;s_searchTimer=0;s_searchPhase=0;s_searchDone=false
    s_atSpawn=false;s_movingToTile=false
    s_burrowPhase=BP_ABOVE;s_burrowTimer=0
    s_strikeX=0;s_strikeY=0;s_isBurrowed=false;s_burrowCooldown=0
    s_tileSize=ts
end

-- ===========================================================================
-- OnUpdate
-- ===========================================================================
function OnUpdate(entity, dt)
    dt=normDt(dt)
    if s_fsm==nil then OnStart(entity) end
    local player=FindPlayer();if player==nil then return end
    local px,py=GetPos(player);px=tonumber(px) or 0.0;py=tonumber(py) or 0.0
    local ex,ey=GetPos(entity);ex=tonumber(ex) or 0.0;ey=tonumber(ey) or 0.0
    local ts=s_tileSize or getTileSize()
    local ecx=ex+ts*0.5;local ecy=ey+ts*0.5
    local pcx=px+ts*0.25;local pcy=py+ts*0.5
    local bRange=BURROW_RANGE_TILES*ts

    if s_fsm==ST_PATROL then
        if ai_playerInFront(ecx,ecy,px,py,s_dirX,s_dirY) then s_fsm=ST_CHASE;return end
        ai_updatePatrol(entity,dt)

    elseif s_fsm==ST_CHASE then
        if not ai_playerInFront(ecx,ecy,px,py,s_dirX,s_dirY) and not ai_hasLOS(ecx,ecy,px,py) then
            s_fsm=ST_SEARCH;s_searchTimer=0;s_searchPhase=0;s_searchDone=false;s_hasLastSeen=true;return
        end
        if ai_distToPlayer(entity,px,py)<=bRange then
            s_fsm=ST_ATTACK;s_burrowPhase=BP_ABOVE;s_burrowTimer=0;s_isBurrowed=false;return
        end
        ai_updateChase(entity,dt,px,py)

    elseif s_fsm==ST_SEARCH then
        if ai_playerInFront(ecx,ecy,px,py,s_dirX,s_dirY) or ai_hasLOS(ecx,ecy,px,py) then s_fsm=ST_CHASE;return end
        ai_updateSearch(entity,dt)
        if s_searchDone then s_searchDone=false;s_fsm=ST_RETURN;s_movingToTile=false;s_atSpawn=false end

    elseif s_fsm==ST_RETURN then
        if ai_playerInFront(ecx,ecy,px,py,s_dirX,s_dirY) then s_fsm=ST_CHASE;return end
        ai_moveToSpawn(entity,dt);if s_atSpawn then s_fsm=ST_PATROL end

    elseif s_fsm==ST_ATTACK then
        local dist=ai_distToPlayer(entity,px,py)
        local dx=pcx-ecx;local dy=pcy-ecy

        if s_burrowPhase==BP_ABOVE then
            if not ai_hasLOS(ecx,ecy,px,py) then
                s_fsm=ST_SEARCH;s_searchTimer=0;s_searchPhase=0;s_searchDone=false;s_hasLastSeen=true;return
            end
            if s_burrowCooldown>0 then s_burrowCooldown=s_burrowCooldown-dt end
            if dist>bRange then
                -- Reposition closer (was dead code before — now reachable)
                local ndx,ndy=ai_norm(dx,dy);local speed=BURROW_SPEED
                if boxBlocked(ecx+ndx*speed*dt,ecy+ndy*speed*dt) then
                    if not boxBlocked(ecx+ndx*speed*dt,ecy) then safeMove(entity,ndx*speed,0,dt)
                    elseif not boxBlocked(ecx,ecy+ndy*speed*dt) then safeMove(entity,0,ndy*speed,dt) end
                else safeMove(entity,ndx*speed,ndy*speed,dt) end
                if math.abs(dx)>math.abs(dy) then s_dirX=dx>0 and 1 or -1;s_dirY=0 else s_dirX=0;s_dirY=dy>0 and 1 or -1 end
            elseif s_burrowCooldown<=0 then
                s_strikeX=pcx;s_strikeY=pcy
                s_burrowTimer=TELEGRAPH_DUR;s_burrowPhase=BP_TELE;s_isBurrowed=false
            end

        elseif s_burrowPhase==BP_TELE then
            s_burrowTimer=s_burrowTimer-dt
            if s_burrowTimer<=0 then s_burrowTimer=BURROW_TRAVEL_TIME;s_burrowPhase=BP_BURIED;s_isBurrowed=true end

        elseif s_burrowPhase==BP_BURIED then
            s_burrowTimer=s_burrowTimer-dt
            if s_burrowTimer<=0 then
                SetPos(entity, s_strikeX-ts*0.5, s_strikeY-ts*0.5)
                s_isBurrowed=false
                s_burrowTimer=STRIKE_DUR;s_burrowPhase=BP_STRIKE
            end

        elseif s_burrowPhase==BP_STRIKE then
            s_burrowTimer=s_burrowTimer-dt
            if s_burrowTimer<=0 then s_burrowTimer=COOLDOWN_TIME;s_burrowPhase=BP_COOL end

        elseif s_burrowPhase==BP_COOL then
            s_burrowTimer=s_burrowTimer-dt
            if s_burrowTimer<=0 then s_burrowPhase=BP_ABOVE;s_burrowCooldown=0;return end
            if dist<bRange*0.5 and dist>0.001 then
                local ndx,ndy=ai_norm(-dx,-dy);local slow=BURROW_SPEED*0.3
                if not boxBlocked(ecx+ndx*slow*dt, ecy+ndy*slow*dt) then safeMove(entity,ndx*slow,ndy*slow,dt) end
            end
        end
    end
end