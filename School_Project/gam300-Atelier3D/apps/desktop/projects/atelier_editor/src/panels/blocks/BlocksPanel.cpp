/* !
@file    	    BlocksPanel.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the Blocks window: measuring and drawing the block cards,
    the palette (categories, search, variables), the canvas, and dragging
    blocks so they snap where they fit.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/blocks/BlocksPanel.hpp"

#include "editor/panels/blocks/BlockPreview.hpp"
#include "editor/panels/blocks/ScriptPreview.hpp"
#include "engine/history/SceneCommands.hpp"

#include "editor/Theme.hpp"

#include "engine/input/InputActions.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <string_view>

namespace Editor::Panels
{
    namespace
    {
        using Engine::Logic::Block;
        using Engine::Logic::BlockShape;
        using Engine::Logic::Category;
        using Engine::Logic::InfoOf;
        using Engine::Logic::Op;

        const ImU32 kDark = IM_COL32(60, 60, 75, 255);
        constexpr std::uint32_t kIdleGrey = 0xDAD8D2; // a block on the canvas that is not running, while the game plays

        ImVec2 operator+(ImVec2 a, ImVec2 b)
        {
            return {a.x + b.x, a.y + b.y};
        }

        ImVec2 operator-(ImVec2 a, ImVec2 b)
        {
            return {a.x - b.x, a.y - b.y};
        }

        float DistanceSquared(ImVec2 a, ImVec2 b)
        {
            const ImVec2 d = a - b;
            return d.x * d.x + d.y * d.y;
        }

        bool Inside(ImVec2 point, ImVec2 min, ImVec2 max)
        {
            return point.x >= min.x && point.y >= min.y && point.x < max.x && point.y < max.y;
        }

        bool ContainsNoCase(std::string_view text, std::string_view part)
        {
            const auto lower = [](char c) { return std::tolower(static_cast<unsigned char>(c)); };
            return std::search(text.begin(), text.end(), part.begin(), part.end(),
                               [&lower](char a, char b) { return lower(a) == lower(b); }) != text.end();
        }

        // 0xRRGGBB, lighter or darker by shade
        ImU32 Rgb(std::uint32_t rgb, float shade, float alpha)
        {
            const auto channel = [rgb, shade](int shift)
            { return std::min(255.0f, static_cast<float>((rgb >> shift) & 0xFFu) * shade) / 255.0f; };
            return ImGui::ColorConvertFloat4ToU32(ImVec4(channel(16), channel(8), channel(0), alpha));
        }

        // Halfway between two colours
        std::uint32_t Between(std::uint32_t a, std::uint32_t b)
        {
            const auto channel = [a, b](int shift)
            { return ((((a >> shift) & 0xFFu) + ((b >> shift) & 0xFFu)) / 2u) << shift; };
            return channel(16) | channel(8) | channel(0);
        }

        // A colour laid over a block, so that what is under it still shows
        ImU32 Wash(ImVec4 colour, float alpha)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(colour.x, colour.y, colour.z, alpha));
        }

        // A condition's tag: a box with its corners cut off, by cut
        std::array<ImVec2, 8> Tag(ImVec2 min, ImVec2 max, float cut)
        {
            return {ImVec2(min.x + cut, min.y), ImVec2(max.x - cut, min.y), ImVec2(max.x, min.y + cut),
                    ImVec2(max.x, max.y - cut), ImVec2(max.x - cut, max.y), ImVec2(min.x + cut, max.y),
                    ImVec2(min.x, max.y - cut), ImVec2(min.x, min.y + cut)};
        }

        // What can join a block shows in its shape: a peg under it, a socket on top. A flat edge joins nothing: a hat
        // starts a script, a cap ends one. Values and conditions have their own shapes and slots.
        constexpr float kSocketShade = 0.68f;

        bool HasPeg(BlockShape shape)
        {
            return shape == BlockShape::Hat || shape == BlockShape::Stack;
        }

        bool HasSocket(BlockShape shape)
        {
            return shape == BlockShape::Stack || shape == BlockShape::Cap;
        }

        bool IsRound(BlockShape shape)
        {
            return shape == BlockShape::Condition || shape == BlockShape::Value;
        }

        // The mark at the start of a stack block's label: its category by shape as well as colour
        Theme::Icon GlyphOf(Category category)
        {
            switch (category)
            {
            case Category::Events: return Theme::Icon::Flag;
            case Category::Motion: return Theme::Icon::Arrow;
            case Category::Looks: return Theme::Icon::Eye;
            case Category::Loops: return Theme::Icon::Rotate;
            case Category::Conditions: return Theme::Icon::Diamond;
            case Category::Math: return Theme::Icon::Plus;
            case Category::Variables: return Theme::Icon::Box;
            case Category::Sensing: return Theme::Icon::Waves;
            case Category::Physics: return Theme::Icon::Move;
            case Category::Screen: return Theme::Icon::Logo;
            case Category::Sound: return Theme::Icon::Play;
            case Category::Game: return Theme::Icon::Home;
            }
            return Theme::Icon::Plus;
        }

        // A label split into words and fields: 'w' a word, 'n' number, 'k' key, 't' text, 'v' variable, 'c' slot
        struct Token
        {
            char kind = 'w';
            std::string_view word;
            std::size_t index = 0; // which number / slot
        };

        std::vector<Token> Tokens(std::string_view label);

        // A block's label; a My Block shows its inputs after its name: "Shoot damage [25] range [100]"
        std::vector<Token> TokensOf(const Block& block, const Engine::Logic::Library* library)
        {
            std::vector<Token> tokens = Tokens(InfoOf(block.op).label);
            const Engine::Logic::LibraryBlock* saved =
                block.op == Op::Call && library ? library->Find(block.text) : nullptr;
            if (saved)
            {
                for (std::size_t i = 0; i < saved->inputs.size() && i < block.numbers.size(); ++i)
                {
                    tokens.push_back({.kind = 'w', .word = saved->inputs[i]});
                    tokens.push_back({.kind = 'n', .index = i});
                }
            }
            return tokens;
        }

        std::vector<Token> Tokens(std::string_view label)
        {
            std::vector<Token> tokens;
            std::size_t numbers = 0;
            std::size_t slots = 0;
            for (std::size_t i = 0; i < label.size();)
            {
                if (label[i] == ' ')
                {
                    ++i;
                    continue;
                }
                if (label[i] == '%' && i + 1 < label.size())
                {
                    const char kind = label[i + 1];
                    tokens.push_back({kind, {}, kind == 'n' ? numbers++ : kind == 'c' ? slots++ : 0});
                    i += 2;
                    continue;
                }
                std::size_t end = label.find_first_of(" %", i);
                end = end == std::string_view::npos ? label.size() : end;
                tokens.push_back({'w', label.substr(i, end - i), 0});
                i = end;
            }
            return tokens;
        }

        std::string NumberText(float value)
        {
            std::array<char, 32> text{};
            std::snprintf(text.data(), text.size(), "%g", static_cast<double>(value));
            return text.data();
        }

        // What a text-like field shows: a variable field with no variable chosen says so
        std::string FieldText(const Block& block, char kind)
        {
            return kind == 'v' && block.text.empty() ? std::string("(variable)") : block.text;
        }

        const Block& EmptySlot()
        {
            static const Block empty;
            return empty;
        }
    }

    BlocksPanel::BlocksPanel() : Panel("Blocks") {}

    BlocksPanel::~BlocksPanel() = default;

    void BlocksPanel::SetTarget(Engine::Scene::GameObject* object, const Engine::Logic::LogicRunner* runner)
    {
        const std::uint32_t id = object ? object->id : 0;
        if (id != objectId_)
        {
            selectedBlocks_.clear();
            boxStart_.reset();
            boxed_.clear();
            deleteSelected_ = false;
            // Each object keeps where its canvas was looked at; a drag in progress cannot follow to another object
            scrolls_[objectId_] = scroll_;
            const auto saved = scrolls_.find(id);
            scroll_ = saved != scrolls_.end() ? saved->second : ImVec2(24.0f, 24.0f);
            drag_.reset();
            pick_.reset();
            menuAction_.reset();
        }
        object_ = object;
        objectId_ = id;
        runner_ = runner;

        highlights_ = {};
        running_.clear();
        problems_.clear();
        if (object_ && object_->logic)
        {
            for (const auto& problem : Engine::Logic::LogicRunner::ValidateVariables(*object_->logic))
            {
                problems_.emplace(problem.block, problem.message);
            }
        }
        if (runner_ && object_)
        {
            highlights_ = ExecutionHighlight::Collect(runner_->Scripts(objectId_));
            if (object_->logic)
            {
                running_ =
                    ExecutionHighlight::RunningBlocks(*object_->logic, runner_->ActiveBlocks(objectId_), highlights_);
            }
            for (const auto& problem : runner_->Problems(objectId_))
            {
                problems_.emplace(problem.block, problem.message);
            }
        }
    }

    void BlocksPanel::Reveal(std::uint32_t blockId, bool onlyIfHidden)
    {
        revealId_ = blockId;
        revealPending_ = true;
        revealOnlyIfHidden_ = onlyIfHidden;
        revealTime_ = ImGui::GetTime();
    }

    Engine::Logic::LogicDesc& BlocksPanel::Logic()
    {
        if (!object_->logic)
        {
            object_->logic.emplace();
        }
        return *object_->logic;
    }

    ExecutionHighlight::Highlight BlocksPanel::HighlightOf(std::uint32_t id) const
    {
        return ExecutionHighlight::HighlightOf(highlights_, id);
    }

    bool BlocksPanel::IsRunning(std::uint32_t id) const
    {
        return std::binary_search(running_.begin(), running_.end(), id);
    }

    const std::string* BlocksPanel::ProblemOf(std::uint32_t id) const
    {
        const auto found = problems_.find(id);
        return found != problems_.end() ? &found->second : nullptr;
    }

    // ---------------------------------------------------------------- Sizes

    void BlocksPanel::UseMetrics(float k)
    {
        m_.k = k;
        m_.frame = ImGui::GetFrameHeight();
        m_.padX = 10.0f * k;
        m_.padY = 5.0f * k;
        m_.gap = 5.0f * k;
        m_.minRow = m_.frame + 2.0f * m_.padY;
        m_.corner = 7.0f * k;
        m_.chip = 4.0f * k;
        m_.seam = 3.0f * k;
        m_.spine = 5.0f * k;
        m_.cap = 5.0f * k;
        m_.lip = 3.0f * k;
        m_.joint = 6.0f * k;
        m_.jointX = 24.0f * k;
        m_.arm = 10.0f * k;
        m_.armBottom = 10.0f * k;
        m_.emptyBody = 16.0f * k;
        m_.roundPad = 8.0f * k;
        m_.emptySlot = 32.0f * k;
    }

    float BlocksPanel::FieldWidth(const Block& block, char kind, std::size_t index) const
    {
        if (block.op == Op::Call && kind == 't')
        {
            return ImGui::CalcTextSize(block.text.empty() ? "(my block)" : block.text.c_str()).x;
        }
        const float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
        switch (kind)
        {
        case 'n':
            return std::max(ImGui::CalcTextSize(NumberText(block.numbers[index]).c_str()).x + padding, 24.0f * m_.k);
        case 'k':
        case 'v': return ImGui::CalcTextSize(FieldText(block, kind).c_str()).x + padding + m_.frame * 0.8f;
        case 't': return std::max(ImGui::CalcTextSize(block.text.c_str()).x + padding + 4.0f * m_.k, 40.0f * m_.k);
        default: return 0.0f;
        }
    }

    ImVec2 BlocksPanel::RowSize(const Block& block) const
    {
        // Stack blocks start with their category's mark (round blocks are too small for one)
        float width = IsRound(InfoOf(block.op).shape) ? 0.0f : ImGui::GetFontSize() + m_.gap;
        float height = m_.frame;
        bool first = true;
        for (const Token& token : TokensOf(block, library_))
        {
            width += first ? 0.0f : m_.gap;
            first = false;
            if (token.kind == 'w')
            {
                width += ImGui::CalcTextSize(token.word.data(), token.word.data() + token.word.size()).x;
                continue;
            }
            // A slot, or a number with a value block in it: as big as what is in it
            const Block* inner = nullptr;
            if (token.kind == 'c')
            {
                inner = token.index < block.slots.size() ? &block.slots[token.index] : &EmptySlot();
            }
            else if (token.kind == 'n' && token.index < block.inputs.size() && block.inputs[token.index].id != 0)
            {
                inner = &block.inputs[token.index];
            }
            if (inner)
            {
                const ImVec2 size = RoundSize(*inner);
                width += size.x;
                height = std::max(height, size.y);
            }
            else
            {
                width += FieldWidth(block, token.kind, token.index);
            }
        }
        return {width, height};
    }

    ImVec2 BlocksPanel::RoundSize(const Block& block) const
    {
        // An empty slot is a default Block: id 0 and not round (a palette block has id 0 too, but is round)
        if (block.id == 0 && !IsRound(InfoOf(block.op).shape))
        {
            return {m_.emptySlot, m_.frame};
        }
        const ImVec2 row = RowSize(block);
        const float height = row.y + 2.0f * 3.0f * m_.k;
        return {std::max(row.x + 2.0f * m_.roundPad, height * 1.6f), height};
    }

    float BlocksPanel::HeaderHeight(const Block& block) const
    {
        return std::max(m_.minRow, RowSize(block).y + 2.0f * m_.padY);
    }

    ImVec2 BlocksPanel::BlockSize(const Block& block) const
    {
        const Engine::Logic::BlockInfo& info = InfoOf(block.op);
        if (IsRound(info.shape))
        {
            return RoundSize(block);
        }
        const float header = HeaderHeight(block);
        float width = std::max(RowSize(block).x + 2.0f * m_.padX, 40.0f * m_.k);
        float height = header;
        if (info.shape == BlockShape::Hat)
        {
            width = std::max(width, 90.0f * m_.k);
        }
        if (info.hasBody)
        {
            // The card holds what is inside it, however wide that is
            const float inset = m_.arm + m_.seam * 2.0f;
            height += std::max(m_.emptyBody, StackSize(block.body).y);
            width = std::max({width, inset + 60.0f * m_.k, inset + StackSize(block.body).x});
            if (info.hasElse)
            {
                height += m_.minRow * 0.8f + std::max(m_.emptyBody, StackSize(block.elseBody).y);
                width = std::max(width, inset + StackSize(block.elseBody).x);
            }
            height += m_.armBottom;
        }
        return {width, height};
    }

    ImVec2 BlocksPanel::StackSize(const std::vector<Block>& blocks) const
    {
        ImVec2 size{0.0f, 0.0f};
        for (const Block& block : blocks)
        {
            const ImVec2 each = BlockSize(block);
            size.x = std::max(size.x, each.x);
            size.y += each.y;
        }
        return size;
    }

    // The width of the widest block in the stack
    float BlocksPanel::StackExtent(const std::vector<Block>& blocks) const
    {
        return StackSize(blocks).x; // a container card is as wide as what is inside it
    }

    // ---------------------------------------------------------------- Drawing

    ImU32 BlocksPanel::Fill(const Block& block, Mode mode, float shade) const
    {
        if (mode == Mode::Ghost && shadow_)
        {
            // An ink shadow on the light canvas
            return ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::kInk.x, Theme::kInk.y, Theme::kInk.z, 0.18f));
        }
        // A stack under no "when" block never runs, so it is drawn faint
        const float alpha = mode == Mode::Ghost ? 0.85f : mode == Mode::Canvas && loose_ ? 0.5f : 1.0f;
        const std::uint32_t colour = Engine::Logic::CategoryColor(InfoOf(block.op).category);
        if (mode == Mode::Canvas && runner_ && !IsRunning(block.id))
        {
            // While the game plays only what is running has its colour. Paused, the block Step runs next is halfway
            // there; running, that block changes every frame, and would only flicker
            const bool next = runner_->IsPaused() && HighlightOf(block.id) == ExecutionHighlight::Highlight::Next;
            return Rgb(next ? Between(colour, kIdleGrey) : kIdleGrey, shade, alpha);
        }
        return Rgb(colour, shade, alpha);
    }

    ImU32 BlocksPanel::LabelOn(const Block& block, Mode mode) const
    {
        // Quieter on a grey block, so that the blocks running stand out by their words too
        const bool idle = mode == Mode::Canvas && runner_ && !IsRunning(block.id);
        return ImGui::GetColorU32(idle ? Theme::kTextMuted : Theme::kInk);
    }

    void BlocksPanel::DrawJoint(const Block& block, ImVec2 at, Mode mode, float shade)
    {
        drawList_->PathArcTo(ImVec2(at.x + m_.jointX, at.y), m_.joint, 0.0f, std::numbers::pi_v<float>, 12);
        drawList_->PathFillConvex(Fill(block, mode, shade));
    }

    void BlocksPanel::Mark(const Block& block, ImVec2 min, ImVec2 max, Mode mode)
    {
        if (mode != Mode::Canvas)
        {
            return;
        }
        // Just revealed from the Logic window: a white wash that fades away
        constexpr double kRevealSeconds = 1.5;
        const double age = ImGui::GetTime() - revealTime_;
        if (block.id == revealId_ && age < kRevealSeconds)
        {
            drawList_->AddRectFilled(
                min, max, Wash(Theme::kPanel, 0.7f * static_cast<float>(1.0 - age / kRevealSeconds)), m_.chip);
        }
        // A problem: a red badge on its corner, which shows on a block of any colour
        if (ProblemOf(block.id))
        {
            const ImVec2 centre(max.x - 2.0f * m_.k, min.y + 2.0f * m_.k);
            const ImVec2 size = ImGui::CalcTextSize("!");
            drawList_->AddCircleFilled(centre, 8.0f * m_.k, ImGui::GetColorU32(Theme::kError));
            drawList_->AddText(ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f), IM_COL32(255, 255, 255, 255),
                               "!");
        }
        // A breakpoint: a flat red tab on the block's left edge, as if clipped to the card. Inside a loop it lies
        // over the rail, beside the block it belongs to.
        if (breakpoints_.contains(block.id))
        {
            const float inset = 3.0f * m_.k;
            const auto rule = breakpointRules_.find({objectId_, block.id});
            const bool off = rule != breakpointRules_.end() && !rule->second.enabled;
            drawList_->AddRectFilled(ImVec2(min.x - 8.0f * m_.k, min.y + inset), ImVec2(min.x, max.y - m_.seam - inset),
                                     ImGui::GetColorU32(off ? Theme::kTextMuted : Theme::kError), m_.chip,
                                     ImDrawFlags_RoundCornersLeft);
        }
    }

    void BlocksPanel::DrawStack(std::vector<Block>& list, ImVec2 at, Mode mode, bool topLevel,
                                Engine::Logic::Script* script)
    {
        float y = at.y;
        std::vector<float> joins; // where each block meets the next
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (mode == Mode::Canvas)
            {
                stackTargets_.push_back({&list, i, ImVec2(at.x, y), topLevel, script});
            }
            const float height = BlockSize(list[i]).y;
            DrawBlock(list[i], ImVec2(at.x, y), mode, Place{.list = &list, .index = i});
            y += height;
            joins.push_back(y);
        }
        if (mode == Mode::Canvas)
        {
            stackTargets_.push_back({&list, list.size(), ImVec2(at.x, y), topLevel, script});
        }

        // Joined blocks share one unbroken spine across the seams, each half in its own block's colour. Stacks that
        // are only near each other have a break in theirs, so what is joined shows at a glance.
        if (mode == Mode::Ghost && shadow_)
        {
            return;
        }
        for (std::size_t i = 0; i + 1 < list.size(); ++i)
        {
            const float middle = joins[i] - m_.seam * 0.5f;
            const float reach = m_.corner + m_.seam; // past the cards' rounded corners
            drawList_->AddRectFilled(ImVec2(at.x, middle - reach), ImVec2(at.x + m_.spine, middle),
                                     Fill(list[i], mode, 0.82f));
            drawList_->AddRectFilled(ImVec2(at.x, middle), ImVec2(at.x + m_.spine, middle + reach),
                                     Fill(list[i + 1], mode, 0.82f));
        }

        // A peg under every block something can join, last so that it lies over the block under it, and a socket on
        // top of a stack something can join above. Blocks inside another have its peg in their socket.
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (HasPeg(InfoOf(list[i].op).shape))
            {
                DrawJoint(list[i], ImVec2(at.x, joins[i] - m_.seam), mode, 1.0f);
            }
        }
        if ((topLevel || mode == Mode::Ghost) && !list.empty() && HasSocket(InfoOf(list.front().op).shape))
        {
            DrawJoint(list.front(), at, mode, kSocketShade);
        }
    }

    void BlocksPanel::DrawBlock(Block& block, ImVec2 at, Mode mode, const Place& place)
    {
        const Engine::Logic::BlockInfo& info = InfoOf(block.op);
        if (IsRound(info.shape))
        {
            DrawRound(block, at, mode, place);
            return;
        }
        ImGui::PushID(static_cast<int>(block.id));
        if (mode == Mode::Canvas)
        {
            positions_[block.id] =
                ImVec2((at.x - canvasMin_.x - scroll_.x) / m_.k, (at.y - canvasMin_.y - scroll_.y) / m_.k);
        }

        const ImVec2 size = BlockSize(block);
        const float x = at.x;
        const float w = size.x;
        const float header = HeaderHeight(block);
        const float top = at.y;
        const float r = m_.corner;

        float bottom = top + header;
        float bodyTop = 0.0f;
        float bodyEnd = 0.0f;
        float elseRow = 0.0f;
        float elseTop = 0.0f;
        if (info.hasBody)
        {
            bodyTop = bottom;
            bottom += std::max(m_.emptyBody, StackSize(block.body).y);
            if (info.hasElse)
            {
                elseRow = bottom;
                elseTop = bottom + m_.minRow * 0.8f;
                bottom = elseTop + std::max(m_.emptyBody, StackSize(block.elseBody).y);
            }
            bodyEnd = bottom;
            bottom += m_.armBottom;
        }

        // A flat card, a seam short of the block under it. A block with blocks inside is a tinted card holding them,
        // with a header, a rail down the left and a foot; a hat has a dark tab behind its icon, the others a spine
        const float cardBottom = bottom - m_.seam;
        const ImU32 fill = Fill(block, mode);
        if (mode == Mode::Ghost && shadow_)
        {
            drawList_->AddRectFilled(ImVec2(x, top), ImVec2(x + w, cardBottom), fill, r);
        }
        else
        {
            const float headerBottom = info.hasBody ? top + header : cardBottom;
            const float tab = info.shape == BlockShape::Hat ? m_.padX + ImGui::GetFontSize() + m_.gap * 0.5f : m_.spine;
            if (info.hasBody)
            {
                const ImU32 alpha = (fill >> IM_COL32_A_SHIFT) & 0xFFu;
                const ImU32 tint = (fill & ~IM_COL32_A_MASK) | ((alpha * 90u / 255u) << IM_COL32_A_SHIFT);
                drawList_->AddRectFilled(ImVec2(x, top), ImVec2(x + w, cardBottom), tint, r);
                drawList_->AddRectFilled(ImVec2(x, headerBottom), ImVec2(x + m_.arm, bodyEnd), fill);
                drawList_->AddRectFilled(ImVec2(x, bodyEnd), ImVec2(x + w, cardBottom), fill, r,
                                         ImDrawFlags_RoundCornersBottom);
                if (info.hasElse)
                {
                    drawList_->AddRectFilled(ImVec2(x, elseRow), ImVec2(x + w, elseTop), fill);
                }
            }
            drawList_->AddRectFilled(ImVec2(x, top), ImVec2(x + w, headerBottom), fill, r,
                                     info.hasBody ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
            drawList_->AddRectFilled(ImVec2(x, top), ImVec2(x + tab, headerBottom), Fill(block, mode, 0.82f), r,
                                     info.hasBody ? ImDrawFlags_RoundCornersTopLeft : ImDrawFlags_RoundCornersLeft);
            // A darker lip along the bottom lifts the card off the canvas; a script ends at a cap's solid foot
            const bool cap = info.shape == BlockShape::Cap;
            const float foot = !cap ? m_.lip : info.hasBody ? m_.armBottom - m_.seam : m_.cap;
            drawList_->AddRectFilled(ImVec2(x, cardBottom - foot), ImVec2(x + w, cardBottom),
                                     Fill(block, mode, cap ? 0.7f : 0.82f), r, ImDrawFlags_RoundCornersBottom);
        }
        Mark(block, ImVec2(x, top), ImVec2(x + w, top + header), mode);

        Grab(block, at, ImVec2(x + w, top + header), mode, place, at, "##head");
        if (info.hasBody)
        {
            Grab(block, ImVec2(x, top + header), ImVec2(x + m_.arm, bottom - m_.armBottom), mode, place, at, "##arm");
            Grab(block, ImVec2(x, bottom - m_.armBottom), ImVec2(x + w, bottom), mode, place, at, "##foot");
            if (info.hasElse)
            {
                Grab(block, ImVec2(x, elseRow), ImVec2(x + w, elseTop), mode, place, at, "##else");
            }
        }
        if (mode == Mode::Canvas)
        {
            DrawBlockMenu(block, place);
        }
        if (!(mode == Mode::Ghost && shadow_))
        {
            DrawRow(block, x + m_.padX, top + header * 0.5f, mode);
            if (info.hasElse)
            {
                drawList_->AddText(ImVec2(x + m_.padX, elseRow + (elseTop - elseRow - ImGui::GetFontSize()) * 0.5f),
                                   LabelOn(block, mode), "else");
            }
        }
        if (info.hasBody)
        {
            DrawStack(block.body, ImVec2(x + m_.arm, bodyTop), mode, false, nullptr);
            if (info.hasElse)
            {
                DrawStack(block.elseBody, ImVec2(x + m_.arm, elseTop), mode, false, nullptr);
            }
            // Its own pegs, into what is inside it: with nothing there, they show that blocks go here
            if (!(mode == Mode::Ghost && shadow_))
            {
                DrawJoint(block, ImVec2(x + m_.arm, bodyTop), mode, 1.0f);
                if (info.hasElse)
                {
                    DrawJoint(block, ImVec2(x + m_.arm, elseTop), mode, 1.0f);
                }
            }
        }
        ImGui::PopID();
    }

    // A condition is a tag with cut corners (it fits the tag-shaped slots of ifs and loops), a value a chip (it fits
    // numbers)
    void BlocksPanel::DrawRound(Block& block, ImVec2 at, Mode mode, const Place& place)
    {
        ImGui::PushID(static_cast<int>(block.id));
        if (mode == Mode::Canvas)
        {
            positions_[block.id] =
                ImVec2((at.x - canvasMin_.x - scroll_.x) / m_.k, (at.y - canvasMin_.y - scroll_.y) / m_.k);
        }
        const ImVec2 size = RoundSize(block);
        const float half = size.y * 0.5f;
        if (InfoOf(block.op).shape == BlockShape::Condition)
        {
            const auto tag = Tag(at, at + size, size.y * 0.24f);
            drawList_->AddConvexPolyFilled(tag.data(), static_cast<int>(tag.size()), Fill(block, mode));
        }
        else
        {
            drawList_->AddRectFilled(at, at + size, Fill(block, mode), m_.chip);
        }
        Mark(block, at, at + size, mode);
        Grab(block, at, at + size, mode, place, at, "##round");
        if (mode == Mode::Canvas)
        {
            DrawBlockMenu(block, place);
        }
        if (!(mode == Mode::Ghost && shadow_))
        {
            DrawRow(block, at.x + m_.roundPad, at.y + half, mode);
        }
        ImGui::PopID();
    }

    void BlocksPanel::DrawRow(Block& block, float x, float centreY, Mode mode)
    {
        const float fontSize = ImGui::GetFontSize();
        const Engine::Logic::BlockInfo& info = InfoOf(block.op);
        if (!IsRound(info.shape))
        {
            Theme::DrawIcon(drawList_, GlyphOf(info.category), ImVec2(x + fontSize * 0.5f, centreY), fontSize,
                            LabelOn(block, mode));
            x += fontSize + m_.gap;
        }
        for (const Token& token : TokensOf(block, library_))
        {
            if (token.kind == 'w')
            {
                drawList_->AddText(ImVec2(x, centreY - fontSize * 0.5f), LabelOn(block, mode), token.word.data(),
                                   token.word.data() + token.word.size());
                x += ImGui::CalcTextSize(token.word.data(), token.word.data() + token.word.size()).x + m_.gap;
                continue;
            }
            if (token.kind == 'c')
            {
                if (token.index >= block.slots.size())
                {
                    continue;
                }
                Block& slot = block.slots[token.index];
                if (slot.id == 0)
                {
                    // An empty slot: a darker tag a condition can be dropped into
                    const ImVec2 min(x, centreY - m_.frame * 0.5f);
                    const ImVec2 max(x + m_.emptySlot, centreY + m_.frame * 0.5f);
                    const auto tag = Tag(min, max, m_.frame * 0.24f);
                    drawList_->AddConvexPolyFilled(tag.data(), static_cast<int>(tag.size()), Fill(block, mode, 0.7f));
                    if (mode == Mode::Canvas)
                    {
                        slotTargets_.push_back({&slot, min, max});
                    }
                    x = max.x + m_.gap;
                }
                else
                {
                    const ImVec2 size = RoundSize(slot);
                    DrawRound(slot, ImVec2(x, centreY - size.y * 0.5f), mode, Place{.slot = &slot});
                    x += size.x + m_.gap;
                }
                continue;
            }
            if (token.kind == 'n' && token.index < block.inputs.size() && block.inputs[token.index].id != 0)
            {
                // A value block in place of the number
                Block& input = block.inputs[token.index];
                const ImVec2 size = RoundSize(input);
                DrawRound(input, ImVec2(x, centreY - size.y * 0.5f), mode, Place{.slot = &input});
                x += size.x + m_.gap;
                continue;
            }

            const float width = FieldWidth(block, token.kind, token.index);
            const ImVec2 min(x, centreY - m_.frame * 0.5f);
            const ImVec2 max(x + width, centreY + m_.frame * 0.5f);
            DrawField(block, token.kind, token.index, min, max, mode);
            if (token.kind == 'n' && mode == Mode::Canvas && token.index < block.inputs.size())
            {
                inputTargets_.push_back({&block.inputs[token.index], min, max});
            }
            x = max.x + m_.gap;
        }
    }

    void BlocksPanel::DrawField(Block& block, char kind, std::size_t index, ImVec2 min, ImVec2 max, Mode mode)
    {
        const float width = max.x - min.x;
        const float centreY = (min.y + max.y) * 0.5f;
        if (block.op == Op::Call && kind == 't')
        {
            const std::string name = block.text.empty() ? std::string("(my block)") : block.text;
            drawList_->AddText(ImVec2(min.x, centreY - ImGui::GetFontSize() * 0.5f), IM_COL32_WHITE, name.c_str());
            return;
        }
        const bool menu = kind == 'k' || kind == 'v'; // a pick-one field, with an arrow
        if (mode == Mode::Canvas)
        {
            ImGui::PushID(static_cast<int>(index) * 16 + kind);
            ImGui::SetCursorScreenPos(min);
            ImGui::SetNextItemWidth(width);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, m_.chip);
            // Locked while playing, but still at full strength so the values stay readable
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            ImGui::BeginDisabled(Locked());
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f); // white on the block's colour is its edge
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(235, 240, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, kDark);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(235, 240, 255, 255));
            constexpr int kColours = 6;
            if (kind == 'n')
            {
                float value = block.numbers[index];
                if (ImGui::InputFloat("##value", &value, 0.0f, 0.0f, "%g"))
                {
                    block.numbers[index] = value;
                }
                ImGui::PopStyleColor(kColours);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !drag_)
                {
                    ImGui::SetTooltip("Type a number, or drop a value block here");
                }
            }
            else if (menu)
            {
                const std::string shown = FieldText(block, kind);
                const bool open = ImGui::BeginCombo("##pick", shown.c_str(), ImGuiComboFlags_HeightLarge);
                ImGui::PopStyleColor(kColours);
                if (open)
                {
                    if (kind == 'k')
                    {
                        for (const Engine::Input::InputBinding& binding : Engine::Input::AllBindings())
                        {
                            const std::string name = Engine::Input::BindingName(binding);
                            if (ImGui::Selectable(name.c_str(), name == block.text))
                            {
                                block.text = name;
                            }
                        }
                    }
                    else if (object_ && object_->logic && !object_->logic->variables.empty())
                    {
                        for (const Engine::Logic::Variable& variable : object_->logic->variables)
                        {
                            if (ImGui::Selectable(variable.name.c_str(), variable.name == block.text))
                            {
                                block.text = variable.name;
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No variables yet: make one in the Variables blocks");
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                std::array<char, 128> text{};
                block.text.copy(text.data(), text.size() - 1);
                if (ImGui::InputText("##text", text.data(), text.size()))
                {
                    block.text = text.data();
                }
                ImGui::PopStyleColor(kColours);
            }
            ImGui::EndDisabled();
            ImGui::PopStyleVar(3);
            ImGui::PopID();
            return;
        }

        // Off the canvas (palette, dragging): just what it holds
        drawList_->AddRectFilled(min, max, IM_COL32(255, 255, 255, mode == Mode::Ghost ? 220 : 255), m_.chip);
        const std::string value = kind == 'n' ? NumberText(block.numbers[index]) : FieldText(block, kind);
        const float textWidth = ImGui::CalcTextSize(value.c_str()).x;
        const float textX = menu ? min.x + ImGui::GetStyle().FramePadding.x : min.x + (width - textWidth) * 0.5f;
        drawList_->AddText(ImVec2(textX, centreY - ImGui::GetFontSize() * 0.5f), kDark, value.c_str());
        if (menu)
        {
            const float s = m_.frame * 0.18f;
            const ImVec2 c(max.x - m_.frame * 0.4f, centreY);
            drawList_->AddTriangleFilled(ImVec2(c.x - s, c.y - s * 0.5f), ImVec2(c.x + s, c.y - s * 0.5f),
                                         ImVec2(c.x, c.y + s * 0.7f), kDark);
        }
    }

    void BlocksPanel::Grab(const Block& block, ImVec2 min, ImVec2 max, Mode mode, const Place& place, ImVec2 blockMin,
                           const char* id)
    {
        if (mode == Mode::Ghost || max.x - min.x < 1.0f || max.y - min.y < 1.0f)
        {
            return;
        }
        ImGui::SetCursorScreenPos(min);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(id, max - min);
        if (mode == Mode::Canvas && ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl && !io.KeyShift)
            {
                selectedBlocks_ = {block.id};
            }
            else if (!selectedBlocks_.erase(block.id))
            {
                selectedBlocks_.insert(block.id);
            }
        }
        // A C block has several grab areas: the box and Select All see them as one
        if (mode == Mode::Canvas && place.slot == nullptr)
        {
            const auto [bounds, added] = blockBounds_.try_emplace(block.id, min, max);
            if (!added)
            {
                auto& [low, high] = bounds->second;
                low = ImVec2(std::min(low.x, min.x), std::min(low.y, min.y));
                high = ImVec2(std::max(high.x, max.x), std::max(high.y, max.y));
            }
        }

        if (mode == Mode::Canvas && (selectedBlocks_.contains(block.id) || boxed_.contains(block.id)))
        {
            drawList_->AddRectFilled(min, max, Wash(Theme::kSelect, 0.3f), m_.chip);
        }
        // The block the hovered script's preview is on now (E / Q step through them), outlined so it can be followed
        if (mode == Mode::Canvas && place.slot == nullptr && previewer_.ShownBlock() == &block)
        {
            drawList_->AddRectFilled(min, max, Wash(Theme::kRun, 0.35f), m_.chip);
            drawList_->AddRect(min, max, ImGui::GetColorU32(Theme::kRun), m_.chip, ImDrawFlags_None, 3.0f * m_.k);
        }
        // A stack block or hat (a value in a slot runs as part of its block)
        if (mode == Mode::Canvas && place.slot == nullptr && onClickBlock_ &&
            ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            onClickBlock_(objectId_, block.id);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !drag_)
        {
            const Engine::Logic::BlockInfo& info = InfoOf(block.op);
            const Engine::Logic::LibraryBlock* saved =
                block.op == Op::Call && library_ ? library_->Find(block.text) : nullptr;
            if (saved)
            {
                std::string inside;
                for (std::size_t i = 0; i < saved->blocks.size() && i < 8; ++i)
                {
                    inside += "\n  " + Engine::Logic::Describe(saved->blocks[i]);
                }
                if (saved->blocks.size() > 8)
                {
                    inside += "\n  ...";
                }
                ImGui::SetTooltip("%s: %zu blocks%s\n\nDouble-click to open", saved->name.c_str(),
                                  static_cast<std::size_t>(Engine::Logic::CountBlocks(
                                      Engine::Logic::LogicDesc{.scripts = {{.blocks = saved->blocks}}})),
                                  inside.c_str());
            }
            else if (ImGui::BeginTooltip())
            {
                // What the block does, shown with the block's own numbers (a palette block of zeros: examples), then
                // in words
                // On the canvas the object itself acts the block out; in the palette (and a block of the library's), Atti
                const bool library = libraryEdit_ && object_ == libraryEdit_->object.get();
                const Engine::Scene::RenderableDesc* look = mode == Mode::Canvas && !library ? look_ : nullptr;
                if (mode == Mode::Canvas && place.list && ShowsScript(block.op))
                {
                    // What it really runs: the blocks under a hat, what is inside a loop or if
                    const std::span<const Block> stack(*place.list);
                    const bool hat = info.shape == BlockShape::Hat;
                    const bool inScript = !stack.empty() && InfoOf(stack.front().op).shape == BlockShape::Hat;
                    const ScriptPreview script = BuildScriptPreview(
                        block, inScript ? stack.first(place.index) : std::span<const Block>{},
                        hat ? stack.subspan(place.index + 1) : std::span<const Block>{}, Logic().variables, library_);
                    previewer_.DrawScript(script, block.id, projectDir_, look, look ? animator_ : nullptr);
                }
                else if (mode != Mode::Palette)
                {
                    // The palette's tooltips only say what a block does: its preview plays once it is on the canvas
                    previewer_.Draw(block, projectDir_, look, look ? animator_ : nullptr);
                }
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kPreviewWidth * uiScale_);
                ImGui::TextUnformatted(std::string(info.help).c_str());
                if (mode == Mode::Canvas && loose_)
                {
                    ImGui::Spacing();
                    ImGui::TextColored(Theme::kTextMuted, "Not under a \"when\" block, so it does not run");
                }
                if (const std::string* problem = mode == Mode::Canvas ? ProblemOf(block.id) : nullptr)
                {
                    ImGui::Spacing();
                    ImGui::TextColored(Theme::kErrorText, "Problem: %s", problem->c_str());
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f * uiScale_) && !drag_ &&
            !pick_ && object_ && !Locked())
        {
            pick_ = Pick{.place = place,
                         .grab = ImGui::GetIO().MouseClickedPos[0] - blockMin,
                         .fromPalette = mode == Mode::Palette,
                         .op = block.op,
                         .text = block.text};
        }
        if (block.op == Op::Call && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            library_ && library_->Find(block.text))
        {
            openLibraryEdit_ = block.text;
        }
        if (mode == Mode::Palette && block.op == Op::Call && ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            libraryMenuFor_ = block.text;
            openLibraryMenu_ = true;
        }
        // A C block has several grab areas: each opens the block's one menu (drawn once, by DrawBlockMenu)
        if (mode == Mode::Canvas)
        {
            ImGui::OpenPopupOnItemClick("##menu", ImGuiPopupFlags_MouseButtonRight);
        }
    }

    void BlocksPanel::DrawBlockMenu(const Block& block, const Place& place)
    {
        if (ImGui::BeginPopup("##menu"))
        {
            ImGui::TextDisabled("%s", Engine::Logic::Describe(block).c_str());
            ImGui::Separator();
            if (block.op == Op::Call && library_ &&
                ImGui::MenuItem("Open", "double-click", false, library_->Find(block.text) != nullptr))
            {
                openLibraryEdit_ = block.text;
            }
            if (block.op == Op::Call && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("See and change the blocks it runs, in a window of their own");
            }
            // Right-clicked in a selection of several: the menu acts on all of them
            const std::size_t several = selectedBlocks_.contains(block.id) ? selectedBlocks_.size() : 0;
            if (several > 1)
            {
                if (ImGui::MenuItem(std::format("Duplicate {} Blocks", several).c_str(), nullptr, false, !Locked()))
                {
                    menuAction_ = MenuAction{.kind = MenuAction::Kind::DuplicateSelected};
                }
                if (ImGui::MenuItem(std::format("Delete {} Blocks", several).c_str(), "Del", false, !Locked()))
                {
                    menuAction_ = MenuAction{.kind = MenuAction::Kind::DeleteSelected};
                }
            }
            else
            {
                if (ImGui::MenuItem("Duplicate", nullptr, false, !Locked()))
                {
                    menuAction_ = MenuAction{.place = place, .kind = MenuAction::Kind::Duplicate};
                }
                if (ImGui::MenuItem("Delete Block", nullptr, false, !Locked()))
                {
                    menuAction_ = MenuAction{.place = place, .kind = MenuAction::Kind::Delete};
                }
            }
            // The library: save this block and those under it as one block, or put a library block's blocks back
            if (place.list && library_)
            {
                ImGui::Separator();
                if (ImGui::MenuItem("Save as My Block..."))
                {
                    saveBlockId_ = block.id;
                    libraryName_.fill('\0');
                    if (block.op == Op::Call)
                    {
                        block.text.copy(libraryName_.data(), libraryName_.size() - 1);
                    }
                    openSave_ = true;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Saves this block and the blocks under it (after a \"when\" block) as one block "
                                      "you can use anywhere, and puts that block here in their place");
                }
                if (block.op == Op::Call && ImGui::MenuItem("Unpack", nullptr, false, library_->Find(block.text)))
                {
                    menuAction_ = MenuAction{.place = place, .kind = MenuAction::Kind::Unpack};
                }
                if (block.op == Op::Call && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Puts a copy of the blocks it runs here instead (your block keeps its own)");
                }
            }
            // Stack blocks and hats only: a value in a slot runs as part of its block
            // Not in a My Block's window: put one on the My Block in a script, and step through it from there
            if (place.slot == nullptr && onToggleBreakpoint_ && !libraryEdit_ &&
                ImGui::MenuItem("Breakpoint", nullptr, breakpoints_.contains(block.id)))
            {
                onToggleBreakpoint_(objectId_, block.id);
            }
            if (place.slot == nullptr && onBreakpointRule_ && !libraryEdit_ && breakpoints_.contains(block.id))
            {
                const auto known = breakpointRules_.find({objectId_, block.id});
                Engine::Logic::BreakpointRule rule =
                    known != breakpointRules_.end() ? known->second : Engine::Logic::BreakpointRule{};
                if (ImGui::Checkbox("Enabled", &rule.enabled))
                {
                    onBreakpointRule_(objectId_, block.id, rule);
                }
                if (breakpointConditionBlock_ != block.id)
                {
                    breakpointConditionBlock_ = block.id;
                    std::snprintf(breakpointCondition_.data(), breakpointCondition_.size(), "%s",
                                  rule.condition.c_str());
                }
                if (ImGui::InputTextWithHint("##stopWhen", "Only when: Speed > 5", breakpointCondition_.data(),
                                             breakpointCondition_.size()))
                {
                    rule.condition = breakpointCondition_.data();
                    onBreakpointRule_(objectId_, block.id, rule);
                }
                if (!rule.condition.empty() && !Engine::Logic::ParseBreakpointCondition(rule.condition))
                {
                    ImGui::TextColored(Theme::kError, "Write it as: variable > number");
                }
            }
            ImGui::EndPopup();
        }
    }

    void BlocksPanel::DrawVariables()
    {
        if (!object_)
        {
            return;
        }
        const float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Make a Variable", ImVec2(width, 0.0f)))
        {
            variableName_.fill('\0');
            variableValue_ = 0.0f;
            variableTrueFalse_ = false;
            ImGui::OpenPopup("##newVariable");
        }
        if (ImGui::BeginPopup("##newVariable"))
        {
            ImGui::TextUnformatted("New variable");
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(180.0f * uiScale_);
            const bool enter = ImGui::InputTextWithHint("##name", "Name, e.g. score", variableName_.data(),
                                                        variableName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Checkbox("True / false", &variableTrueFalse_);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Shown as true or false, e.g. IsAlive. Blocks still use it as a number: 0 is false, "
                                  "anything else is true.");
            }
            if (variableTrueFalse_)
            {
                bool startsTrue = variableValue_ != 0.0f;
                if (ImGui::Checkbox("Starts true", &startsTrue))
                {
                    variableValue_ = startsTrue ? 1.0f : 0.0f;
                }
            }
            else
            {
                ImGui::SetNextItemWidth(180.0f * uiScale_);
                ImGui::InputFloat("Starts at", &variableValue_, 0.0f, 0.0f, "%g");
            }
            const std::string name = variableName_.data();
            const bool ok =
                Engine::Logic::CanAddVariable(object_->logic ? *object_->logic : Engine::Logic::LogicDesc{}, name);
            if (!ok && !name.empty())
            {
                ImGui::TextDisabled("That name is already used");
            }
            ImGui::BeginDisabled(!ok);
            if (ImGui::Button("Make") || (enter && ok))
            {
                using Engine::Logic::VariableKind;
                Engine::Logic::Variable made{.name = name, .value = variableValue_};
                if (variableTrueFalse_)
                {
                    made.value = variableValue_ != 0.0f ? 1.0f : 0.0f;
                    made.kind = VariableKind::TrueFalse;
                }
                Logic().variables.push_back(std::move(made));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        if (!object_->logic || object_->logic->variables.empty())
        {
            ImGui::TextDisabled("No variables yet");
            return;
        }

        // Each variable: its starting value (or, while playing, what it is now), rename and delete
        const auto live = runner_ ? runner_->Variables(objectId_) : std::vector<std::pair<std::string, double>>{};
        std::optional<std::size_t> remove;
        std::vector<Engine::Logic::Variable>& variables = object_->logic->variables;
        for (std::size_t i = 0; i < variables.size(); ++i)
        {
            Engine::Logic::Variable& variable = variables[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Theme::kWarningText, "%s", variable.name.c_str());
            if (ImGui::BeginPopupContextItem("##variableMenu"))
            {
                if (ImGui::MenuItem("Rename..."))
                {
                    renaming_ = variable.name;
                    openRename_ = true;
                    variableName_.fill('\0');
                    variable.name.copy(variableName_.data(), variableName_.size() - 1);
                }
                // The kind is part of the logic: changed while playing, it would restart the object's scripts
                bool trueFalse = variable.kind == Engine::Logic::VariableKind::TrueFalse;
                if (ImGui::MenuItem("Show as true / false", nullptr, &trueFalse, !runner_))
                {
                    variable.kind =
                        trueFalse ? Engine::Logic::VariableKind::TrueFalse : Engine::Logic::VariableKind::Number;
                }
                if (runner_ && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("Change this when the game is stopped");
                }
                if (ImGui::MenuItem("Delete"))
                {
                    remove = i;
                }
                ImGui::EndPopup();
            }
            if (openRename_ && renaming_ == variable.name)
            {
                ImGui::OpenPopup("##renameVariable");
                openRename_ = false;
            }
            if (ImGui::BeginPopup("##renameVariable"))
            {
                ImGui::SetNextItemWidth(180.0f * uiScale_);
                const bool enter = ImGui::InputText("##rename", variableName_.data(), variableName_.size(),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::Button("Rename") || enter)
                {
                    Engine::Logic::RenameVariable(*object_->logic, renaming_, variableName_.data());
                    renaming_.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            else if (!openRename_ && renaming_ == variable.name)
            {
                renaming_.clear(); // closed without renaming
            }
            ImGui::SameLine();
            const auto now = std::find_if(live.begin(), live.end(),
                                          [&variable](const auto& each) { return each.first == variable.name; });
            if (now != live.end())
            {
                ImGui::TextColored(Theme::kSuccessText, "= %s",
                                   Engine::Logic::ValueText(variable.kind, now->second).c_str());
            }
            else if (variable.kind == Engine::Logic::VariableKind::TrueFalse)
            {
                bool startsTrue = variable.value != 0.0f;
                if (ImGui::Checkbox("starts true##start", &startsTrue))
                {
                    variable.value = startsTrue ? 1.0f : 0.0f;
                }
            }
            else
            {
                ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() -
                                                     ImGui::GetStyle().ItemSpacing.x,
                                                 40.0f * uiScale_));
                ImGui::DragFloat("##start", &variable.value, 0.1f, 0.0f, 0.0f, "starts at %g");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                remove = i;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                const std::size_t uses = Engine::Logic::CountUses(*object_->logic, variable.name);
                ImGui::SetTooltip(uses == 0 ? "Delete this variable"
                                            : "Delete this variable (%zu blocks use it: they will report a problem)",
                                  uses);
            }
            ImGui::PopID();
        }
        if (remove)
        {
            variables.erase(variables.begin() + static_cast<std::ptrdiff_t>(*remove));
        }
        ImGui::Separator();
    }

    void BlocksPanel::LibraryChanged(std::string_view from, std::string_view to)
    {
        if (onLibraryChanged_)
        {
            onLibraryChanged_(from, to);
        }
    }

    void BlocksPanel::DrawLibrary()
    {
        if (!library_)
        {
            ImGui::TextDisabled("Open a game to make blocks of your own");
            return;
        }
        if (library_->blocks.empty())
        {
            ImGui::TextWrapped("Nothing saved yet. Right-click a block in a script and pick Save as My Block: it and "
                               "the blocks under it become one block, ready to use in any object's scripts.");
            return;
        }
        ImGui::TextDisabled("Double-click one to open it, right-click for more, drag it into a script");
    }

    void BlocksPanel::DrawLibraryMenus()
    {
        if (!library_)
        {
            return;
        }
        if (openLibraryMenu_)
        {
            ImGui::OpenPopup("##libraryMenu");
            openLibraryMenu_ = false;
        }
        std::optional<std::string> remove;
        if (ImGui::BeginPopup("##libraryMenu"))
        {
            const Engine::Logic::LibraryBlock* saved = library_->Find(libraryMenuFor_);
            ImGui::TextDisabled("%s", libraryMenuFor_.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Open", "double-click", false, saved != nullptr))
            {
                openLibraryEdit_ = libraryMenuFor_;
            }
            if (ImGui::MenuItem("Rename...", nullptr, false, saved != nullptr))
            {
                renamingLibrary_ = libraryMenuFor_;
                libraryName_.fill('\0');
                saved->name.copy(libraryName_.data(), libraryName_.size() - 1);
                openLibraryRename_ = true;
            }
            if (ImGui::MenuItem("Inputs...", nullptr, false, saved != nullptr))
            {
                inputsOfLibrary_ = libraryMenuFor_;
                libraryInputs_.fill('\0');
                Engine::Logic::InputsText(saved->inputs).copy(libraryInputs_.data(), libraryInputs_.size() - 1);
                openLibraryInputs_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", nullptr, false, saved != nullptr))
            {
                remove = libraryMenuFor_;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Scripts that use it will report a problem");
            }
            ImGui::EndPopup();
        }
        if (openLibraryInputs_)
        {
            ImGui::OpenPopup("##libraryInputs");
            openLibraryInputs_ = false;
        }
        if (ImGui::BeginPopup("##libraryInputs"))
        {
            ImGui::TextUnformatted("The numbers this block takes, up to 4, separated by commas");
            ImGui::TextDisabled("Inside the block, read them as variables with these names");
            ImGui::SetNextItemWidth(260.0f * uiScale_);
            const bool enter = ImGui::InputTextWithHint("##inputs", "damage, range", libraryInputs_.data(),
                                                        libraryInputs_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("OK") || enter)
            {
                for (Engine::Logic::LibraryBlock& saved : library_->blocks)
                {
                    if (saved.name == inputsOfLibrary_)
                    {
                        saved.inputs = Engine::Logic::ParseInputs(libraryInputs_.data());
                    }
                }
                LibraryChanged();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (openLibraryRename_)
        {
            ImGui::OpenPopup("##renameLibrary");
            openLibraryRename_ = false;
        }
        if (ImGui::BeginPopup("##renameLibrary"))
        {
            ImGui::SetNextItemWidth(200.0f * uiScale_);
            const bool enter = ImGui::InputText("##rename", libraryName_.data(), libraryName_.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("Rename") || enter)
            {
                const std::string from = renamingLibrary_;
                const std::string to = libraryName_.data();
                if (Engine::Logic::RenameLibraryBlock(*library_, from, to))
                {
                    LibraryChanged(from, to);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (remove)
        {
            std::erase_if(library_->blocks,
                          [&remove](const Engine::Logic::LibraryBlock& saved) { return saved.name == *remove; });
            LibraryChanged();
        }
    }

    void BlocksPanel::DrawSaveToLibrary()
    {
        if (openSave_)
        {
            ImGui::OpenPopup("##saveToLibrary");
            openSave_ = false;
        }
        if (!ImGui::BeginPopup("##saveToLibrary"))
        {
            return;
        }
        ImGui::TextUnformatted("Save as My Block");
        ImGui::TextDisabled("This block and the ones under it become one block");
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(220.0f * uiScale_);
        const bool enter = ImGui::InputTextWithHint("##name", "Name, e.g. Movement Logic", libraryName_.data(),
                                                    libraryName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string name = libraryName_.data();
        const bool valid = Engine::Logic::IsValidLibraryName(name);
        if (valid && library_ && library_->Find(name))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Replaces your block called %s", name.c_str());
        }
        ImGui::BeginDisabled(!valid);
        if ((ImGui::Button("Save") || (enter && valid)) && object_ && object_->logic && library_)
        {
            // The block it was picked on, wherever it is now: a script's own list, or inside a C block
            const auto find = [this](const auto& self,
                                     std::vector<Block>& list) -> std::pair<std::vector<Block>*, std::size_t>
            {
                for (std::size_t i = 0; i < list.size(); ++i)
                {
                    if (list[i].id == saveBlockId_)
                    {
                        return {&list, i};
                    }
                    for (std::vector<Block>* inner : {&list[i].body, &list[i].elseBody})
                    {
                        if (const auto found = self(self, *inner); found.first)
                        {
                            return found;
                        }
                    }
                }
                return {nullptr, 0};
            };
            for (Engine::Logic::Script& script : object_->logic->scripts)
            {
                if (const auto [list, index] = find(find, script.blocks); list)
                {
                    const std::uint32_t callId = Engine::Logic::NextBlockId(*object_->logic);
                    if (Engine::Logic::SaveToLibrary(*library_, *list, index, name, callId))
                    {
                        LibraryChanged();
                    }
                    break;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    void BlocksPanel::DrawPalette(float width, float height)
    {
        ImGui::BeginChild("##palette", ImVec2(width, height), ImGuiChildFlags_Borders);
        paletteMin_ = ImGui::GetWindowPos();
        paletteMax_ = paletteMin_ + ImGui::GetWindowSize();
        drawList_ = ImGui::GetWindowDrawList();
        UseMetrics(uiScale_);

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", "Search blocks", search_.data(), search_.size());
        const std::string_view search(search_.data());

        // The categories, each with its colour: three to a row, or two when the longest name would be cut off
        if (search.empty())
        {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            float widest = 0.0f;
            for (const Category category : Engine::Logic::kCategories)
            {
                const std::string label = "     " + std::string(Engine::Logic::CategoryName(category));
                widest = std::max(widest, ImGui::CalcTextSize(label.c_str()).x);
            }
            const float avail = ImGui::GetContentRegionAvail().x;
            const std::size_t columns = (avail - spacing * 2.0f) / 3.0f >= widest ? 3 : 2;
            const float cell = (avail - spacing * static_cast<float>(columns - 1)) / static_cast<float>(columns);
            for (std::size_t i = 0; i < Engine::Logic::kCategories.size(); ++i)
            {
                const Category category = Engine::Logic::kCategories[i];
                if (i % columns != 0)
                {
                    ImGui::SameLine();
                }
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const std::string label = "     " + std::string(Engine::Logic::CategoryName(category));
                if (ImGui::Selectable(label.c_str(), category_ == category, 0, ImVec2(cell, 0.0f)))
                {
                    category_ = category;
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("%s", std::string(Engine::Logic::CategoryHelp(category)).c_str());
                }
                const float dot = ImGui::GetTextLineHeight() * 0.32f;
                drawList_->AddCircleFilled(
                    ImVec2(at.x + dot + 3.0f * uiScale_, at.y + ImGui::GetTextLineHeight() * 0.5f), dot,
                    Rgb(Engine::Logic::CategoryColor(category), 1.0f, 1.0f));
            }
            ImGui::Separator();
            ImGui::TextWrapped("%s", std::string(Engine::Logic::CategoryHelp(category_)).c_str());
            if (category_ == Category::Variables)
            {
                DrawVariables();
            }
            if (category_ == Category::Library)
            {
                DrawLibrary();
            }
        }
        if (!object_)
        {
            ImGui::TextDisabled("Select an object first");
        }

        // The blocks, drawn as they will look: the category's, or every block the search matches
        const std::vector<Engine::Logic::Variable> noVariables;
        const std::vector<Engine::Logic::Variable>& variables =
            object_ && object_->logic ? object_->logic->variables : noVariables;
        std::vector<Block> shown;
        for (const Engine::Logic::BlockInfo& info : Engine::Logic::AllBlocks())
        {
            Block block = Engine::Logic::MakeBlock(info.op, 0);
            if (info.op == Op::Call)
            {
                // One per library block, ready to drop into a script
                if (library_ && (!search.empty() || category_ == Category::Library))
                {
                    for (const Engine::Logic::LibraryBlock* saved :
                         Engine::Logic::LibraryBlocksMatching(*library_, search))
                    {
                        block.text = saved->name;
                        shown.push_back(block);
                    }
                }
                continue;
            }
            const bool matches = search.empty()
                                     ? info.category == category_
                                     : ContainsNoCase(info.label, search) ||
                                           ContainsNoCase(Engine::Logic::CategoryName(info.category), search) ||
                                           ContainsNoCase(info.help, search);
            if (!matches)
            {
                continue;
            }
            if (info.op == Op::VariableValue)
            {
                // One per variable, ready to drop into a number
                for (const Engine::Logic::Variable& variable : variables)
                {
                    block.text = variable.name;
                    shown.push_back(block);
                }
                continue;
            }
            if (Engine::Logic::HasVariableField(info.op))
            {
                if (variables.empty())
                {
                    continue; // nothing to set or change yet
                }
                block.text = variables.front().name;
            }
            shown.push_back(std::move(block));
        }
        if (shown.empty() && !search.empty())
        {
            ImGui::TextDisabled("No block matches \"%s\"", search_.data());
        }

        ImGui::BeginChild("##paletteBlocks", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        drawList_ = ImGui::GetWindowDrawList();
        ImVec2 at = ImGui::GetCursorScreenPos() + ImVec2(4.0f * uiScale_, 6.0f * uiScale_);
        const ImVec2 start = at;
        float widest = 0.0f;
        for (std::size_t i = 0; i < shown.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i) + 1000);
            // A My Block can be opened with no object selected; it just cannot be dragged into one
            ImGui::BeginDisabled(object_ == nullptr && shown[i].op != Op::Call);
            DrawBlock(shown[i], at, Mode::Palette, Place{});
            // The same joints as on the canvas, so what joins what can be seen before picking a block up
            const BlockShape shape = InfoOf(shown[i].op).shape;
            if (HasSocket(shape))
            {
                DrawJoint(shown[i], at, Mode::Palette, kSocketShade);
            }
            if (HasPeg(shape))
            {
                DrawJoint(shown[i], ImVec2(at.x, at.y + BlockSize(shown[i]).y - m_.seam), Mode::Palette, 1.0f);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
            const ImVec2 size = BlockSize(shown[i]);
            widest = std::max(widest, size.x);
            at.y += size.y + 10.0f * uiScale_;
        }
        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(ImVec2(widest, std::max(at.y - start.y, 1.0f)));
        ImGui::EndChild();
        drawList_ = ImGui::GetWindowDrawList();
        DrawLibraryMenus();
        ImGui::EndChild();
    }

    void BlocksPanel::DrawCanvas(ImVec2 size)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kHeader); // the editor theme's block canvas colour
        ImGui::BeginChild("##canvas", size, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleColor();
        canvasMin_ = ImGui::GetWindowPos();
        canvasMax_ = canvasMin_ + ImGui::GetWindowSize();
        drawList_ = ImGui::GetWindowDrawList();
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * zoom_);
        UseMetrics(uiScale_ * zoom_);

        const ImGuiIO& io = ImGui::GetIO();
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        deleteSelected_ = !selectedBlocks_.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
                          !io.WantTextInput && !Locked() && !drag_ && !pick_ &&
                          !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                          ImGui::IsKeyPressed(ImGuiKey_Delete, false);

        // Zoom with Ctrl + wheel around the pointer; the wheel alone scrolls (Shift: sideways)
        if (hovered && io.MouseWheel != 0.0f && !drag_)
        {
            if (io.KeyCtrl)
            {
                const float before = uiScale_ * zoom_;
                zoom_ = std::clamp(zoom_ * (1.0f + 0.1f * io.MouseWheel), 0.4f, 2.5f);
                const float after = uiScale_ * zoom_;
                const ImVec2 point = io.MousePos - canvasMin_ - scroll_;
                scroll_ = io.MousePos - canvasMin_ - ImVec2(point.x * after / before, point.y * after / before);
            }
            else if (io.KeyShift)
            {
                scroll_.x += io.MouseWheel * 40.0f * uiScale_;
            }
            else
            {
                scroll_.y += io.MouseWheel * 40.0f * uiScale_;
            }
        }

        // The background: dots to show it moves, and the place to drag (or middle-drag) to look around
        const float spacing = 24.0f * m_.k;
        const ImU32 dot = ImGui::ColorConvertFloat4ToU32(Theme::kBorderStrong);
        const ImVec2 firstDot(canvasMin_.x + std::fmod(scroll_.x, spacing),
                              canvasMin_.y + std::fmod(scroll_.y, spacing));
        const int columns = static_cast<int>(std::ceil((canvasMax_.x - firstDot.x) / spacing));
        const int rows = static_cast<int>(std::ceil((canvasMax_.y - firstDot.y) / spacing));
        for (int column = 0; column < columns; ++column)
        {
            for (int row = 0; row < rows; ++row)
            {
                const ImVec2 at(firstDot.x + static_cast<float>(column) * spacing,
                                firstDot.y + static_cast<float>(row) * spacing);
                drawList_->AddCircleFilled(at, 1.2f * uiScale_, dot);
            }
        }
        ImGui::SetCursorScreenPos(canvasMin_);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##background", ImVec2(std::max(size.x, 1.0f), std::max(size.y, 1.0f)),
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
        // Shift + drag boxes blocks instead of moving the canvas
        if (ImGui::IsItemActivated() && io.KeyShift && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            boxStart_ = io.MousePos;
        }
        if (ImGui::IsItemActive() && !drag_ && !boxStart_ &&
            (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
        {
            scroll_ = scroll_ + io.MouseDelta;
        }
        // A click on the canvas (not a drag round it) lets go of the selection
        const float clickSlop = 4.0f * uiScale_;
        if (ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !boxStart_ && !io.KeyCtrl &&
            !io.KeyShift && io.MouseDragMaxDistanceSqr[0] < clickSlop * clickSlop)
        {
            selectedBlocks_.clear();
        }
        if (ImGui::BeginPopupContextItem("##canvasMenu"))
        {
            if (ImGui::MenuItem("Select All", "Ctrl+A", false, !blockBounds_.empty()))
            {
                SelectAll();
            }
            if (ImGui::MenuItem("Clean up Blocks", nullptr, false, object_ && object_->logic))
            {
                cleanUp_ = true;
            }
            if (ImGui::MenuItem("Reset Zoom"))
            {
                zoom_ = 1.0f;
            }
            ImGui::EndPopup();
        }

        stackTargets_.clear();
        slotTargets_.clear();
        inputTargets_.clear();
        positions_.clear();
        blockBounds_.clear();
        if (object_ && object_->logic)
        {
            const std::optional<std::size_t> picked = SteppedScript();
            for (std::size_t i = 0; i < object_->logic->scripts.size(); ++i)
            {
                Engine::Logic::Script& script = object_->logic->scripts[i];
                const ImVec2 at = canvasMin_ + scroll_ + ImVec2(script.x * m_.k, script.y * m_.k);
                if (picked == i)
                {
                    // The script Step runs, tinted behind it as wide as its widest block, with a label above
                    const float pad = 6.0f * m_.k;
                    const ImVec2 extent(StackExtent(script.blocks), StackSize(script.blocks).y);
                    drawList_->AddRectFilled(at - ImVec2(pad, pad), at + extent + ImVec2(pad, pad),
                                             Wash(Theme::kRun, 0.25f), pad);
                    drawList_->AddText(at - ImVec2(pad, pad + ImGui::GetFontSize() + 2.0f * uiScale_),
                                       ImGui::ColorConvertFloat4ToU32(Theme::kRun), "Step runs this script");
                }
                // A library block's stack runs wherever it is used, with no "when" block of its own
                const bool library = libraryEdit_ && object_ == libraryEdit_->object.get();
                loose_ =
                    !library && !script.blocks.empty() && InfoOf(script.blocks.front().op).shape != BlockShape::Hat;
                DrawStack(script.blocks, at, Mode::Canvas, true, &script);
                loose_ = false;
            }
        }
        if (!object_ || !object_->logic || object_->logic->scripts.empty())
        {
            const char* hint = object_ ? "Drag blocks here from the left, starting with a yellow \"when\" block"
                                       : "Select an object to give it blocks";
            // Wrapped to the canvas, so a narrow canvas still shows all of it
            const float wrap = std::max(size.x - 32.0f * uiScale_, 1.0f);
            const ImVec2 text = ImGui::CalcTextSize(hint, nullptr, false, wrap);
            drawList_->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                               canvasMin_ + ImVec2((size.x - text.x) * 0.5f, (size.y - text.y) * 0.5f),
                               ImGui::ColorConvertFloat4ToU32(Theme::kTextMuted), hint, nullptr, wrap);
        }

        // Blocks gone since they were picked (undone, say) are not picked any more
        std::erase_if(selectedBlocks_, [this](std::uint32_t id) { return !positions_.contains(id); });

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false) && !io.WantTextInput &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        {
            SelectAll();
        }

        // The box: what it touches shows as picked while it is dragged, and is added when it is let go
        boxed_.clear();
        if (boxStart_)
        {
            const ImVec2 boxMin(std::min(boxStart_->x, io.MousePos.x), std::min(boxStart_->y, io.MousePos.y));
            const ImVec2 boxMax(std::max(boxStart_->x, io.MousePos.x), std::max(boxStart_->y, io.MousePos.y));
            for (const auto& [id, bounds] : blockBounds_)
            {
                if (bounds.first.x < boxMax.x && bounds.second.x > boxMin.x && bounds.first.y < boxMax.y &&
                    bounds.second.y > boxMin.y)
                {
                    boxed_.insert(id);
                }
            }
            drawList_->AddRectFilled(boxMin, boxMax, Wash(Theme::kSelect, 0.15f));
            drawList_->AddRect(boxMin, boxMax, ImGui::ColorConvertFloat4ToU32(Theme::kSelect));
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                selectedBlocks_.insert(boxed_.begin(), boxed_.end());
                boxed_.clear();
                boxStart_.reset();
            }
        }

        // What can be done with the selection, so it can be found
        if (!selectedBlocks_.empty() && !drag_)
        {
            const std::string words =
                selectedBlocks_.size() > 1
                    ? std::format("{} blocks picked: Delete removes them, right-click one for more",
                                  selectedBlocks_.size())
                    : std::string("Ctrl + click picks more blocks, Shift + drag picks an area, Ctrl + A picks all");
            drawList_->AddText(canvasMin_ + ImVec2(8.0f * uiScale_, 6.0f * uiScale_),
                               ImGui::ColorConvertFloat4ToU32(Theme::kTextMuted), words.c_str());
        }

        ImGui::PopFont();
        ImGui::EndChild();
    }

    void BlocksPanel::SelectAll()
    {
        selectedBlocks_.clear();
        for (const auto& [id, bounds] : blockBounds_)
        {
            selectedBlocks_.insert(id);
        }
    }

    void BlocksPanel::DrawLegend()
    {
        ImDrawList* list = ImGui::GetWindowDrawList();
        const float height = ImGui::GetTextLineHeight();
        const auto swatch = [&](ImU32 fill, const char* label)
        {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const ImVec2 min(at.x, at.y + height * 0.15f);
            const ImVec2 max(at.x + height * 1.4f, at.y + height * 0.85f);
            list->AddRectFilled(min, max, fill, 3.0f);
            ImGui::Dummy(ImVec2(height * 1.4f, height));
            ImGui::SameLine(0.0f, 4.0f * uiScale_);
            ImGui::TextUnformatted(label);
            ImGui::SameLine(0.0f, 12.0f * uiScale_);
        };
        const std::uint32_t kExample = Engine::Logic::CategoryColor(Category::Motion);
        swatch(Rgb(kExample, 1.0f, 1.0f), "running now");
        swatch(Rgb(kIdleGrey, 1.0f, 1.0f), "not running");
        if (runner_->IsPaused())
        {
            swatch(Rgb(Between(kExample, kIdleGrey), 1.0f, 1.0f), "runs next");
        }
        swatch(ImGui::GetColorU32(Theme::kError), "problem (did nothing)");
        ImGui::TextDisabled("(stop to edit blocks)");
    }

    // The script Step runs, while paused: the one the game stopped in
    std::optional<std::size_t> BlocksPanel::SteppedScript() const
    {
        if (!runner_ || !runner_->IsPaused())
        {
            return std::nullopt;
        }
        const Engine::Logic::DebugState& debug = runner_->Debug();
        if (debug.pausedObject != objectId_ || debug.pausedThread < 0)
        {
            return std::nullopt;
        }
        const auto scripts = runner_->Scripts(objectId_);
        const auto index = static_cast<std::size_t>(debug.pausedThread);
        return index < scripts.size() ? std::optional(scripts[index].script) : std::nullopt;
    }

    // ---------------------------------------------------------------- Changes

    bool BlocksPanel::PickUp()
    {
        if (!pick_ || !object_)
        {
            pick_.reset();
            return false;
        }
        const Pick pick = *pick_;
        pick_.reset();
        if (pick.fromPalette)
        {
            const std::uint32_t id = object_->logic ? Engine::Logic::NextBlockId(*object_->logic) : 1;
            Block block = Engine::Logic::MakeBlock(pick.op, id);
            block.text = pick.text;
            drag_ = Drag{.blocks = {std::move(block)}, .grab = pick.grab};
            if (onPluck_)
            {
                onPluck_();
            }
            return false;
        }
        if (pick.place.slot)
        {
            drag_ = Drag{.blocks = {std::move(*pick.place.slot)}, .grab = pick.grab};
            *pick.place.slot = Block{};
        }
        else if (pick.place.list)
        {
            drag_ = Drag{.blocks = Engine::Logic::TakeStack(*pick.place.list, pick.place.index), .grab = pick.grab};
            RemoveEmptyScripts();
        }
        return true;
    }

    void BlocksPanel::DrawDropHints()
    {
        // By the same rules as FindSnap: every place the blocks being dragged could go lights up, so what joins what
        // can be seen without trying it
        if (!drag_)
        {
            return;
        }
        const std::vector<Block>& dragged = drag_->blocks;
        const Block& first = dragged.front();
        const BlockShape shape = InfoOf(first.op).shape;
        drawList_->PushClipRect(canvasMin_, canvasMax_, true);
        if (dragged.size() == 1 && IsRound(shape))
        {
            const bool condition = shape == BlockShape::Condition;
            for (const SlotTarget& target : condition ? slotTargets_ : inputTargets_)
            {
                if (condition ? Engine::Logic::CanFillSlot(*target.slot, first)
                              : Engine::Logic::CanFillInput(*target.slot, first))
                {
                    drawList_->AddRectFilled(target.min, target.max, Wash(Theme::kSelect, 0.3f), m_.chip);
                }
            }
        }
        else
        {
            const ImVec2 size(48.0f * m_.k, 4.0f * m_.k);
            for (const StackTarget& target : stackTargets_)
            {
                if (Engine::Logic::CanInsert(*target.list, target.index, dragged, target.topLevel))
                {
                    const ImVec2 min(target.at.x + m_.spine, target.at.y - size.y * 0.5f - m_.seam * 0.5f);
                    drawList_->AddRectFilled(min, min + size, Wash(Theme::kSelect, 0.9f), size.y * 0.5f);
                }
            }
        }
        drawList_->PopClipRect();
    }

    void BlocksPanel::FindSnap(ImVec2 mouse)
    {
        snapStack_.reset();
        snapSlot_.reset();
        snapInput_.reset();
        if (!drag_ || !object_ || !Inside(mouse, canvasMin_, canvasMax_))
        {
            return;
        }
        const ImVec2 top = mouse - drag_->grab;
        const Block& first = drag_->blocks.front();
        const BlockShape shape = InfoOf(first.op).shape;
        float best = 20.0f * m_.k * 20.0f * m_.k;

        if (drag_->blocks.size() == 1 && IsRound(shape))
        {
            // A condition goes into a tag-shaped slot, a value into a number: its left end meets the slot's
            const ImVec2 point(top.x, top.y + RoundSize(first).y * 0.5f);
            const bool condition = shape == BlockShape::Condition;
            for (const SlotTarget& target : condition ? slotTargets_ : inputTargets_)
            {
                const bool fits = condition ? Engine::Logic::CanFillSlot(*target.slot, first)
                                            : Engine::Logic::CanFillInput(*target.slot, first);
                const float distance =
                    DistanceSquared(point, ImVec2(target.min.x, (target.min.y + target.max.y) * 0.5f));
                if (fits && distance < best)
                {
                    best = distance;
                    (condition ? snapSlot_ : snapInput_) = target;
                }
            }
            return;
        }

        const float height = StackSize(drag_->blocks).y;
        for (const StackTarget& target : stackTargets_)
        {
            if (!Engine::Logic::CanInsert(*target.list, target.index, drag_->blocks, target.topLevel))
            {
                continue;
            }
            // At the top of a script the dragged blocks go above it: their bottom meets its top
            const bool above = target.topLevel && target.index == 0 && !target.list->empty();
            const ImVec2 point = above ? ImVec2(top.x, top.y + height) : top;
            const float distance = DistanceSquared(point, target.at);
            if (distance < best)
            {
                best = distance;
                snapStack_ = target;
            }
        }
    }

    void BlocksPanel::Drop(ImVec2 mouse)
    {
        if (!drag_)
        {
            return;
        }
        Drag drag = std::move(*drag_);
        drag_.reset();
        if (!object_ || Inside(mouse, paletteMin_, paletteMax_))
        {
            return; // back on the palette: deleted
        }
        const SlotTarget* filled = snapSlot_ ? &*snapSlot_ : snapInput_ ? &*snapInput_ : nullptr;
        if (filled)
        {
            *filled->slot = std::move(drag.blocks.front());
            if (onAttach_)
            {
                onAttach_();
            }
            return;
        }
        if (snapStack_)
        {
            const StackTarget& target = *snapStack_;
            if (target.topLevel && target.index == 0 && !target.list->empty() && target.script)
            {
                target.script->y -= StackSize(drag.blocks).y / m_.k; // the script grows upwards
            }
            target.list->insert(target.list->begin() + static_cast<std::ptrdiff_t>(target.index),
                                std::make_move_iterator(drag.blocks.begin()),
                                std::make_move_iterator(drag.blocks.end()));
            if (onAttach_)
            {
                onAttach_();
            }
            return;
        }
        // Not near anything: a new script where it was let go (kept inside the canvas)
        ImVec2 top = mouse - drag.grab;
        top.x = std::clamp(top.x, canvasMin_.x, std::max(canvasMin_.x, canvasMax_.x - 40.0f * m_.k));
        top.y = std::clamp(top.y, canvasMin_.y, std::max(canvasMin_.y, canvasMax_.y - 20.0f * m_.k));
        Logic().scripts.push_back(Engine::Logic::Script{.x = (top.x - canvasMin_.x - scroll_.x) / m_.k,
                                                        .y = (top.y - canvasMin_.y - scroll_.y) / m_.k,
                                                        .blocks = std::move(drag.blocks)});
    }

    void BlocksPanel::RemoveEmptyScripts()
    {
        if (object_ && object_->logic)
        {
            std::erase_if(object_->logic->scripts,
                          [](const Engine::Logic::Script& script) { return script.blocks.empty(); });
        }
    }

    // One under another down the left, in the order they were made
    void BlocksPanel::CleanUp()
    {
        if (!object_ || !object_->logic)
        {
            return;
        }
        float y = 16.0f;
        for (Engine::Logic::Script& script : object_->logic->scripts)
        {
            script.x = 36.0f;
            script.y = y;
            y += StackSize(script.blocks).y / m_.k + 24.0f;
        }
        scroll_ = ImVec2(0.0f, 0.0f);
    }

    void BlocksPanel::DrawContents()
    {
        if (!libraryEdit_)
        {
            DrawEditor();
            if (!openLibraryEdit_.empty())
            {
                BeginLibraryEdit(openLibraryEdit_);
                openLibraryEdit_.clear();
            }
            return;
        }
        // The object's blocks wait under the library block's window
        ImGui::TextDisabled("Changing your block \"%s\"...", libraryEdit_->name.c_str());
        DrawLibraryEdit();
    }

    void BlocksPanel::BeginLibraryEdit(const std::string& name)
    {
        const Engine::Logic::LibraryBlock* saved = library_ ? library_->Find(name) : nullptr;
        if (!saved)
        {
            return;
        }
        // A stand-in object: its one script is the library block's blocks, with the object's variables to pick from
        auto object = std::make_unique<Engine::Scene::GameObject>();
        object->id = std::numeric_limits<std::uint32_t>::max();
        object->name = name;
        Engine::Logic::LogicDesc logic;
        logic.scripts.push_back(Engine::Logic::Script{.x = 16.0f, .y = 16.0f, .blocks = saved->blocks});
        if (object_ && object_->logic)
        {
            logic.variables = object_->logic->variables;
        }
        // Its inputs can be picked like any variable
        for (const std::string& input : saved->inputs)
        {
            if (!Engine::Logic::FindVariable(logic, input))
            {
                logic.variables.push_back({.name = input, .value = 0.0f});
            }
        }
        object->logic = std::move(logic);
        libraryEdit_ = LibraryEdit{.name = name, .object = std::move(object)};
        libraryEdit_->committed = libraryEdit_->object->logic;
        openEditPopup_ = true;
        selectedBlocks_.clear(); // its blocks have ids of their own
        drag_.reset();
        pick_.reset();
        menuAction_.reset();
    }

    void BlocksPanel::DrawLibraryEdit()
    {
        if (!libraryEdit_)
        {
            return;
        }
        LibraryEdit& edit = *libraryEdit_;
        std::optional<Engine::Logic::LogicDesc>& editedLogic = edit.object->logic;
        if (!editedLogic)
        {
            return;
        }
        if (openEditPopup_)
        {
            ImGui::OpenPopup("###LibraryEdit"); // the same id as the popup's "My Block###LibraryEdit"
            openEditPopup_ = false;
        }
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.8f, viewport->WorkSize.y * 0.8f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("My Block###LibraryEdit", nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            libraryEdit_.reset(); // closed (Esc, or the game was closed)
            return;
        }

        // What can be saved: one stack, not starting with a "when" block
        const std::vector<Engine::Logic::Script>& scripts = editedLogic->scripts;
        const char* problem = nullptr;
        if (scripts.empty() || scripts.front().blocks.empty())
        {
            problem = "Drag blocks onto the canvas: your block needs at least one";
        }
        else if (scripts.size() > 1)
        {
            problem = "Put the blocks together in one stack to save them";
        }
        else if (InfoOf(scripts.front().blocks.front().op).shape == Engine::Logic::BlockShape::Hat)
        {
            problem = "Your block cannot start with a \"when\" block: it runs from inside a script";
        }

        Theme::Text(edit.name.c_str(), 18.0f, Theme::kText, true);
        ImGui::SameLine(0.0f, 12.0f * uiScale_);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Saving changes it in every script that uses it. Ctrl+Z undoes, Ctrl+Y redoes");
        const float button = 34.0f * uiScale_;
        const float buttons = Theme::PillWidth("Save", button) + Theme::PillWidth("Cancel", button) + 8.0f * uiScale_;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttons));
        bool save = Theme::PillButton("Save", Theme::kObjects, Theme::kInk, button, 0.0f, false, Theme::Icon::Plus,
                                      problem == nullptr);
        if (problem && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", problem);
        }
        ImGui::SameLine(0.0f, 8.0f * uiScale_);
        bool close = Theme::PillButton("Cancel", Theme::kRaised, Theme::kText, button) ||
                     (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !drag_ && !ImGui::GetIO().WantTextInput);
        if (problem)
        {
            ImGui::TextColored(Theme::kWarningText, "%s", problem);
        }
        ImGui::Separator();

        // The same editor, on the stand-in object (the game's running blocks are not shown here)
        Engine::Scene::GameObject* owner = object_;
        const std::uint32_t ownerId = objectId_;
        const ImVec2 ownerScroll = scroll_;
        const Engine::Logic::LogicRunner* ownerRunner = runner_;
        std::map<std::uint32_t, std::string> ownerProblems = std::move(problems_);
        problems_.clear();
        object_ = edit.object.get();
        objectId_ = object_->id;
        scroll_ = edit.scroll;
        runner_ = nullptr;
        DrawEditor();
        edit.scroll = scroll_;
        object_ = owner;
        objectId_ = ownerId;
        scroll_ = ownerScroll;
        runner_ = ownerRunner;
        problems_ = std::move(ownerProblems);
        // A double-click on a library block inside this one: save or cancel this one first
        openLibraryEdit_.clear();

        // What changed here is one undo step when the drag or the typing is over
        if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !drag_ && !pick_)
        {
            if (editedLogic != edit.committed)
            {
                Engine::Scene::GameObject* standIn = edit.object.get();
                edit.history.Push(std::make_unique<Engine::History::BlockEditCommand>(
                    [standIn] { return &standIn->logic; }, edit.committed, editedLogic));
                edit.committed = editedLogic;
            }
            if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
            {
                const bool undo = ImGui::IsKeyPressed(ImGuiKey_Z, false) && !ImGui::GetIO().KeyShift;
                const bool redo = ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                                  (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyShift);
                if (undo)
                {
                    edit.history.Undo();
                }
                else if (redo)
                {
                    edit.history.Redo();
                }
                edit.committed = editedLogic;
            }
        }

        if (save && problem == nullptr && library_)
        {
            const Engine::Logic::LogicDesc& edited = *editedLogic;
            const auto entry =
                std::find_if(library_->blocks.begin(), library_->blocks.end(),
                             [&edit](const Engine::Logic::LibraryBlock& each) { return each.name == edit.name; });
            if (entry != library_->blocks.end())
            {
                std::vector<Block> blocks = edited.scripts.front().blocks;
                std::uint32_t next = 1; // a library block's blocks have ids of their own
                Engine::Logic::Renumber(blocks, next);
                entry->blocks = std::move(blocks);
                LibraryChanged();
            }
            // Variables made here are the object's
            if (object_)
            {
                for (const Engine::Logic::Variable& variable : edited.variables)
                {
                    if (Engine::Logic::CanAddVariable(Logic(), variable.name))
                    {
                        Logic().variables.push_back(variable);
                    }
                }
            }
            close = true;
        }
        if (close)
        {
            selectedBlocks_.clear();
            drag_.reset();
            pick_.reset();
            menuAction_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (close)
        {
            libraryEdit_.reset();
        }
    }

    void BlocksPanel::DrawEditor()
    {
        // Toolbar: whose blocks (in blue), zoom, and what to do next
        if (object_)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Theme::kSelect, "%s:", object_->name.c_str());
            ImGui::SameLine();
            const std::size_t count = object_->logic ? Engine::Logic::CountBlocks(*object_->logic) : 0;
            const std::size_t variables = object_->logic ? object_->logic->variables.size() : 0;
            ImGui::TextDisabled("%zu blocks, %zu variables", count, variables);
            const float button = 30.0f * uiScale_;
            ImGui::SameLine(0.0f, 12.0f * uiScale_);
            if (Theme::IconButton("##zoomOut", Theme::Icon::Minus, button, "Zoom out"))
            {
                zoom_ = std::max(0.4f, zoom_ - 0.1f);
            }
            ImGui::SameLine(0.0f, 6.0f * uiScale_);
            ImGui::Text("%.0f%%", zoom_ * 100.0f);
            ImGui::SameLine(0.0f, 6.0f * uiScale_);
            if (Theme::IconButton("##zoomIn", Theme::Icon::Plus, button, "Zoom in"))
            {
                zoom_ = std::min(2.5f, zoom_ + 0.1f);
            }
            ImGui::SameLine(0.0f, 8.0f * uiScale_);
            if (Theme::PillButton("Clean up", Theme::kRaised, Theme::kText, button))
            {
                cleanUp_ = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Line the scripts up neatly");
            }
            // While playing: what the colours mean. Otherwise what to do next, when there is room for
            // it on this line (the empty canvas says the same)
            if (runner_)
            {
                ImGui::SameLine(0.0f, 12.0f * uiScale_);
                DrawLegend();
            }
            else
            {
                const char* hint = libraryEdit_ ? "One stack: it runs wherever your block is used"
                                                : "Put blocks under a yellow \"when\" block, then press Run";
                const float gap = 12.0f * uiScale_;
                const float lineEnd = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                if (ImGui::GetItemRectMax().x + gap + ImGui::CalcTextSize(hint).x <= lineEnd)
                {
                    ImGui::SameLine(0.0f, gap);
                    ImGui::TextColored(Theme::kTextMuted, "%s", hint);
                }
            }
        }
        else
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("No object selected");
        }

        // No room yet (e.g. the first frame of a docked layout that was kept hidden at 0 x 0): ImGui does not take
        // zero-sized items, so draw nothing until the window has a size. A drag cannot carry on without the canvas.
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 32.0f * uiScale_ || avail.y < 32.0f * uiScale_)
        {
            drag_.reset();
            pick_.reset();
            menuAction_.reset();
            return;
        }
        const float paletteWidth = std::min(280.0f * uiScale_, avail.x * 0.45f);
        DrawPalette(paletteWidth, avail.y);
        ImGui::SameLine();
        DrawCanvas(ImVec2(std::max(avail.x - paletteWidth - ImGui::GetStyle().ItemSpacing.x, 1.0f), avail.y));

        DrawSaveToLibrary();

        // Changes wait until everything is drawn: they change the lists being drawn
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * zoom_);
        UseMetrics(uiScale_ * zoom_);
        if (PickUp())
        {
            stackTargets_.clear(); // they point into what was just changed
            slotTargets_.clear();
            inputTargets_.clear();
        }
        if (drag_)
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            FindSnap(mouse);

            // Where it would go: a grey shadow, or the slot lit up
            drawList_ = ImGui::GetForegroundDrawList();
            DrawDropHints();
            if (snapStack_)
            {
                const float height = StackSize(drag_->blocks).y;
                const bool above = snapStack_->topLevel && snapStack_->index == 0 && !snapStack_->list->empty();
                shadow_ = true;
                DrawStack(drag_->blocks, above ? snapStack_->at - ImVec2(0.0f, height) : snapStack_->at, Mode::Ghost,
                          false, nullptr);
                shadow_ = false;
            }
            for (const std::optional<SlotTarget>* snap : {&snapSlot_, &snapInput_})
            {
                if (*snap)
                {
                    drawList_->AddRectFilled((*snap)->min, (*snap)->max, Wash(Theme::kSelect, 0.6f), m_.chip);
                }
            }
            // The blocks themselves, under the pointer
            DrawStack(drag_->blocks, mouse - drag_->grab, Mode::Ghost, false, nullptr);
            if (Inside(mouse, paletteMin_, paletteMax_))
            {
                ImGui::SetTooltip("Let go to delete");
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                Drop(mouse);
                snapStack_.reset();
                snapSlot_.reset();
                snapInput_.reset();
            }
        }

        if (deleteSelected_ && object_ && object_->logic && !Locked() && !drag_ && !pick_)
        {
            // By id, because edits and undo can move blocks
            menuAction_ = MenuAction{.kind = MenuAction::Kind::DeleteSelected};
        }

        deleteSelected_ = false;

        if (menuAction_ && object_)
        {
            const MenuAction action = *menuAction_;
            if (action.kind == MenuAction::Kind::Delete)
            {
                selectedBlocks_.clear();
            }
            menuAction_.reset();
            const Place& place = action.place;
            if (action.kind == MenuAction::Kind::DeleteSelected)
            {
                Engine::Logic::RemoveBlocks(Logic(), selectedBlocks_);
                selectedBlocks_.clear();
            }
            else if (action.kind == MenuAction::Kind::DuplicateSelected)
            {
                // Each run of picked blocks as a new script, just below and to the right of where it was
                std::uint32_t next = Engine::Logic::NextBlockId(Logic());
                for (std::vector<Block>& copy : Engine::Logic::CopyBlocks(Logic(), selectedBlocks_))
                {
                    const std::uint32_t original = copy.front().id;
                    Engine::Logic::Renumber(copy, next);
                    const ImVec2 from = positions_.contains(original) ? positions_[original] : ImVec2(16.0f, 16.0f);
                    Logic().scripts.push_back(
                        Engine::Logic::Script{.x = from.x + 24.0f, .y = from.y + 24.0f, .blocks = std::move(copy)});
                }
            }
            if (action.kind == MenuAction::Kind::Unpack)
            {
                std::uint32_t next = Engine::Logic::NextBlockId(Logic());
                if (library_ && place.list)
                {
                    Engine::Logic::UnpackCall(*library_, *place.list, place.index, next);
                }
            }
            else if (action.kind == MenuAction::Kind::Duplicate)
            {
                // The block and those under it, as a new script just below and to the right
                std::vector<Block> copy;
                std::uint32_t original = 0;
                if (place.slot)
                {
                    copy.push_back(*place.slot);
                    original = place.slot->id;
                }
                else if (place.list && place.index < place.list->size())
                {
                    copy.assign(place.list->begin() + static_cast<std::ptrdiff_t>(place.index), place.list->end());
                    original = (*place.list)[place.index].id;
                }
                if (!copy.empty())
                {
                    std::uint32_t next = Engine::Logic::NextBlockId(Logic());
                    Engine::Logic::Renumber(copy, next);
                    const ImVec2 from = positions_.contains(original) ? positions_[original] : ImVec2(16.0f, 16.0f);
                    Logic().scripts.push_back(
                        Engine::Logic::Script{.x = from.x + 24.0f, .y = from.y + 24.0f, .blocks = std::move(copy)});
                }
            }
            else if (place.slot)
            {
                *place.slot = Block{};
            }
            else if (place.list && place.index < place.list->size())
            {
                place.list->erase(place.list->begin() + static_cast<std::ptrdiff_t>(place.index));
                RemoveEmptyScripts();
            }
        }
        if (cleanUp_)
        {
            cleanUp_ = false;
            CleanUp();
        }
        if (revealPending_ && positions_.contains(revealId_))
        {
            revealPending_ = false;
            const ImVec2 at = positions_[revealId_];
            const ImVec2 size = canvasMax_ - canvasMin_;
            // Where the block's corner is on the canvas now, and room for the block itself
            const ImVec2 shown(scroll_.x + at.x * m_.k, scroll_.y + at.y * m_.k);
            const float room = m_.frame * 2.0f;
            const bool inView =
                shown.x >= 0.0f && shown.y >= 0.0f && shown.x + room <= size.x && shown.y + room <= size.y;
            if (!(revealOnlyIfHidden_ && inView))
            {
                scroll_ = ImVec2(size.x * 0.3f - at.x * m_.k, size.y * 0.3f - at.y * m_.k);
            }
        }
        ImGui::PopFont();
    }
}
