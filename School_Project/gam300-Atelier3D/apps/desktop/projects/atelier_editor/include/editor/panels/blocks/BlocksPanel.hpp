/* !
@file    	    BlocksPanel.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Declares BlocksPanel, the Blocks window: a palette and
    canvas where the selected object's logic is built by dragging block
    blocks together.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/BreakpointFile.hpp"

#include <array>

#include "editor/panels/Panel.hpp"
#include "editor/panels/blocks/BlockPreview.hpp"
#include "editor/panels/blocks/ExecutionHighlight.hpp"
#include "engine/history/UndoRedoManager.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/logic/Library.hpp"

#include <imgui/imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Engine::Logic
{
    class LogicRunner;
}

namespace Engine::Scene
{
    struct GameObject;
}

namespace Editor::Panels
{
    // The Blocks window. On the left, the palette: the blocks of one category at a time (Events, Motion, Looks, Loops,
    // Conditions, Math, Variables, Sensing, Health) in their colours, or every block matching the search. On the
    // right, the canvas of the selected object's scripts. Blocks are dragged from the palette onto the canvas and
    // snap together where they fit: blocks are flat cards that stack one under another, a block's peg going into the
    // socket of the one below (a flat edge joins nothing), and dragging lights up every place they could go. C blocks
    // (repeat, if, forever) are cards that hold a stack inside, conditions are tags that fit tag-shaped slots, and
    // value blocks (variables,
    // math, sensing) are chips that fit any number, so a number can be worked out while the game runs. Where a block
    // cannot go (under a "forever", a hat inside a loop, a stack block in a slot) it does not snap. Picking up a block
    // takes the blocks under it along; dropping blocks back on the palette deletes them. While the game plays the
    // canvas is grey, and only the blocks running have their colours.
    //
    // Variables are made, renamed, given a starting value and deleted in the Variables category, which also shows
    // their values while the game runs. Fields are edited in place (numbers, keys, names, messages). Right-click a
    // block to duplicate or delete it, the empty canvas to tidy the scripts; drag the empty canvas (or with the middle
    // mouse) to look around, Ctrl + wheel to zoom. While the game runs, the blocks that are running glow, and a block
    // that ran into a problem has a red badge with the reason in its tooltip. A block's right-click "Breakpoint"
    // marks it with a red dot: the game pauses just before it runs.
    //
    // The panel edits the object it is given directly; the editor's history records each finished drag or edit as
    // one step.
    //
    // A library block (one saved with Save to Library) opens in a window of its own: double-click it, or Open in its
    // menu or in the Library category. Its blocks are edited there with the same palette and canvas, and Save puts
    // them back in the library, for every script that uses it.
    class BlocksPanel : public Panel
    {
    public:
        BlocksPanel();
        ~BlocksPanel() override;

        // The object whose logic is shown (nullptr: none selected), and the running game's logic for highlighting
        // (nullptr: not playing). Called every frame before DrawContents.
        void SetTarget(Engine::Scene::GameObject* object, const Engine::Logic::LogicRunner* runner);
        void SetScale(float uiScale) { uiScale_ = uiScale; }
        // Scrolls the canvas to the block and makes it flash (the Logic window's click). With onlyIfHidden the canvas
        // stays where it is while the block is in view, so stepping does not move the blocks about
        void Reveal(std::uint32_t blockId, bool onlyIfHidden = false);
        // The shown object's breakpoints (a red tab each), set every frame with SetTarget
        void SetBreakpoints(std::set<std::uint32_t> blocks) { breakpoints_ = std::move(blocks); }
        // Called with the object's id and the block's when its right-click "Breakpoint" is clicked
        void SetBreakpointRules(Engine::Logic::BreakpointRules rules) { breakpointRules_ = std::move(rules); }
        void SetOnBreakpointRule(
            std::function<void(std::uint32_t, std::uint32_t, Engine::Logic::BreakpointRule)> onRule)
        {
            onBreakpointRule_ = std::move(onRule);
        }
        void SetOnToggleBreakpoint(std::function<void(std::uint32_t, std::uint32_t)> onToggle)
        {
            onToggleBreakpoint_ = std::move(onToggle);
        }
        // Called with the object's id and the block's when a block on the canvas is clicked (while paused, Step then
        // runs that block's script)
        void SetOnClickBlock(std::function<void(std::uint32_t, std::uint32_t)> onClick)
        {
            onClickBlock_ = std::move(onClick);
        }
        // Called when dragged blocks are let go where they snap onto others: into a stack or a slot
        void SetOnAttach(std::function<void()> onAttach) { onAttach_ = std::move(onAttach); }
        // Called when a new block is pulled out of the palette (as the drag starts)
        void SetOnPluck(std::function<void()> onPluck) { onPluck_ = std::move(onPluck); }
        // What acts the blocks on the canvas out in their previews (LookOf the object; nullptr: Atti), and the game
        // folder its files are in. Called every frame with SetTarget.
        void SetPreviewLook(const std::filesystem::path& projectDir, const Engine::Scene::RenderableDesc* look,
                            const Engine::Scene::AnimatorComponent* animator)
        {
            projectDir_ = projectDir;
            look_ = look;
            animator_ = animator;
        }
        // Where the palette and canvas were drawn last
        ImVec2 PaletteMin() const { return paletteMin_; }
        ImVec2 PaletteMax() const { return paletteMax_; }
        ImVec2 CanvasMin() const { return canvasMin_; }
        ImVec2 CanvasMax() const { return canvasMax_; }
        // Frees the block previews' GPU side while the GL context is alive
        void Shutdown() { previewer_.Clear(); }
        // The game's block library (nullptr: no game open), and what to do once it changed: save it, and when a
        // library block was renamed (from, to), rename the blocks that run it everywhere in the scene
        void SetLibrary(Engine::Logic::Library* library,
                        std::function<void(std::string_view from, std::string_view to)> onChanged)
        {
            library_ = library;
            onLibraryChanged_ = std::move(onChanged);
        }

        void DrawContents() override;
        // A My Block is open in its window, which keeps its own undo
        bool IsEditingMyBlock() const { return libraryEdit_.has_value(); }

    private:
        // The toolbar, palette and canvas of the object being shown (the library block's, in its window)
        void DrawEditor();
        // A library block's window: its blocks on a canvas of their own, with Save and Cancel
        void BeginLibraryEdit(const std::string& name);
        void DrawLibraryEdit();

        enum class Mode
        {
            Canvas,  // the object's blocks: fields are edited, blocks picked up, places to snap to collected
            Palette, // a block to drag out
            Ghost    // the blocks being dragged
        };

        // Where a block on the canvas is: in a list (a script's own, or inside a C block), or in a condition slot or a
        // number's input (slot)
        struct Place
        {
            std::vector<Engine::Logic::Block>* list = nullptr;
            std::size_t index = 0;
            Engine::Logic::Block* slot = nullptr;
        };

        // A place a dragged stack could snap to: before index of list, at the point on screen where its top-left
        // goes (for the top of a script: where its bottom-left goes, above the script)
        struct StackTarget
        {
            std::vector<Engine::Logic::Block>* list = nullptr;
            std::size_t index = 0;
            ImVec2 at;
            bool topLevel = false;
            Engine::Logic::Script* script = nullptr;
        };

        // An empty condition slot, or a number a value block could replace
        struct SlotTarget
        {
            Engine::Logic::Block* slot = nullptr;
            ImVec2 min;
            ImVec2 max;
        };

        struct Drag
        {
            std::vector<Engine::Logic::Block> blocks;
            ImVec2 grab; // from the stack's top-left to the mouse, on screen
        };

        // Sizes on screen at the current zoom, worked out each frame (they follow the font size)
        struct Metrics
        {
            float k = 1.0f;     // canvas units to pixels
            float frame = 0.0f; // height of a field
            float padX = 0.0f;
            float padY = 0.0f;
            float gap = 0.0f;
            float minRow = 0.0f;
            float corner = 0.0f;
            float chip = 0.0f;      // the corners of values and fields
            float seam = 0.0f;      // the gap under a block, before the next
            float spine = 0.0f;     // the dark edge down a block's left, unbroken down blocks that are joined
            float cap = 0.0f;       // the dark foot of a block that ends a script
            float lip = 0.0f;       // the darker edge along the bottom of a card
            float joint = 0.0f;     // the radius of a peg and a socket
            float jointX = 0.0f;    // how far along the block they are
            float arm = 0.0f;       // the rail down the left of a block holding blocks
            float armBottom = 0.0f; // its foot
            float emptyBody = 0.0f; // the inside of such a block with nothing in it
            float roundPad = 0.0f;  // before and after the words of a value or condition
            float emptySlot = 0.0f; // width of an empty condition slot
        };

        void UseMetrics(float k);

        // ---- Sizes
        float FieldWidth(const Engine::Logic::Block& block, char kind, std::size_t index) const;
        ImVec2 RowSize(const Engine::Logic::Block& block) const; // the label row's contents
        // A condition (a tag with cut corners) or a value (a chip); an empty condition slot when id is 0
        ImVec2 RoundSize(const Engine::Logic::Block& block) const;
        ImVec2 BlockSize(const Engine::Logic::Block& block) const;
        ImVec2 StackSize(const std::vector<Engine::Logic::Block>& blocks) const;
        float HeaderHeight(const Engine::Logic::Block& block) const;

        // ---- Drawing (and, on the canvas, the interaction of each block)
        void DrawStack(std::vector<Engine::Logic::Block>& list, ImVec2 at, Mode mode, bool topLevel,
                       Engine::Logic::Script* script);
        void DrawBlock(Engine::Logic::Block& block, ImVec2 at, Mode mode, const Place& place);
        void DrawRound(Engine::Logic::Block& block, ImVec2 at, Mode mode, const Place& place);
        // The label row: words, fields and slots, from x, centred on centreY
        void DrawRow(Engine::Logic::Block& block, float x, float centreY, Mode mode);
        // A white field to type in (or, off the canvas, just its value)
        void DrawField(Engine::Logic::Block& block, char kind, std::size_t index, ImVec2 min, ImVec2 max, Mode mode);
        // The block's grab area: picking it up, its menu and its tooltip
        // The block's right-click menu (Duplicate, Delete, Save to Library, ...), drawn once per block
        void DrawBlockMenu(const Engine::Logic::Block& block, const Place& place);
        void Grab(const Engine::Logic::Block& block, ImVec2 min, ImVec2 max, Mode mode, const Place& place,
                  ImVec2 blockMin, const char* id);
        // The colour of words and icons on the block as it is drawn now
        ImU32 LabelOn(const Engine::Logic::Block& block, Mode mode) const;
        // A peg or socket of the block whose left edge is at at: half a disc hanging down, in the block's colour at shade
        void DrawJoint(const Engine::Logic::Block& block, ImVec2 at, Mode mode, float shade);
        // While dragging: every place the blocks could go, lit up
        void DrawDropHints();
        // Whether the block has its colour while the game plays: it, or a block inside it, is running
        bool IsRunning(std::uint32_t id) const;
        // What goes over a block on the canvas, from min to max: the Logic window's flash, the problem badge and the
        // breakpoint tab
        void Mark(const Engine::Logic::Block& block, ImVec2 min, ImVec2 max, Mode mode);
        // While the game plays: what the colours mean
        void DrawLegend();
        void DrawPalette(float width, float height);
        // The Variables category's own part: Make a Variable, and each variable's value, rename and delete
        void DrawVariables();
        // The width of the widest block of the stack, blocks inside C blocks included
        float StackExtent(const std::vector<Engine::Logic::Block>& blocks) const;
        // The index of the script Step runs while the game is paused, if it is in this object
        std::optional<std::size_t> SteppedScript() const;
        // The Library category's own part: each library block, with Rename and Delete
        void DrawLibrary();
        void DrawLibraryMenus();
        // The Save to Library popup, for the block picked in its menu
        void DrawSaveToLibrary();
        void LibraryChanged(std::string_view from = {}, std::string_view to = {});
        void DrawCanvas(ImVec2 size);
        // The block's fill and outline colours for the mode (the drop preview's shadow is grey)
        ImU32 Fill(const Engine::Logic::Block& block, Mode mode, float shade = 1.0f) const;

        // ---- Changes, done once the frame's blocks are drawn
        bool PickUp(); // true if blocks were taken out of the canvas
        void FindSnap(ImVec2 mouse);
        void Drop(ImVec2 mouse);
        void CleanUp();
        void RemoveEmptyScripts();
        Engine::Logic::LogicDesc& Logic();

        // Click picks one block; Ctrl or Shift + click adds or takes away one, Shift + drag on the canvas boxes
        // several, Ctrl + A picks them all. Delete and the block menu then act on all of them.
        std::set<std::uint32_t> selectedBlocks_;
        bool deleteSelected_ = false;
        void SelectAll();
        // Where each stack block was drawn this frame, for the box and Select All
        std::map<std::uint32_t, std::pair<ImVec2, ImVec2>> blockBounds_;
        std::optional<ImVec2> boxStart_; // the corner of the box being dragged
        std::set<std::uint32_t> boxed_;  // the blocks inside it so far
        // While the game plays the blocks cannot be changed (like the Inspector): the running scripts point into them
        bool Locked() const { return runner_ != nullptr; }
        ExecutionHighlight::Highlight HighlightOf(std::uint32_t id) const;
        const std::string* ProblemOf(std::uint32_t id) const;

        Engine::Scene::GameObject* object_ = nullptr;
        std::uint32_t objectId_ = 0;
        const Engine::Logic::LogicRunner* runner_ = nullptr;
        std::set<std::uint32_t> breakpoints_; // the shown object's (SetBreakpoints)
        std::function<void(std::uint32_t, std::uint32_t)> onToggleBreakpoint_;
        std::function<void(std::uint32_t, std::uint32_t, Engine::Logic::BreakpointRule)> onBreakpointRule_;
        Engine::Logic::BreakpointRules breakpointRules_;
        std::array<char, 64> breakpointCondition_{}; // what the right-click menu's field shows
        std::uint32_t breakpointConditionBlock_ = 0; // the block it is for
        std::function<void(std::uint32_t, std::uint32_t)> onClickBlock_;
        std::function<void()> onAttach_;
        std::function<void()> onPluck_;
        float uiScale_ = 1.0f;
        std::filesystem::path projectDir_;
        const Engine::Scene::RenderableDesc* look_ = nullptr;
        const Engine::Scene::AnimatorComponent* animator_ = nullptr;
        BlockPreviewer previewer_;
        float zoom_ = 1.0f;
        Engine::Logic::Category category_ = Engine::Logic::Category::Events;
        std::array<char, 64> search_{};
        std::map<std::uint32_t, ImVec2> scrolls_; // per object: the canvas's offset on screen
        ImVec2 scroll_{24.0f, 24.0f};

        Metrics m_;
        ImDrawList* drawList_ = nullptr;
        ImVec2 canvasMin_;
        ImVec2 canvasMax_;
        ImVec2 paletteMin_;
        ImVec2 paletteMax_;

        ExecutionHighlight::ScriptHighlights highlights_; // the blocks the scripts just ran, ran before and run next
        std::vector<std::uint32_t> running_;              // sorted: the blocks with their colour while the game plays
        bool loose_ = false; // the stack being drawn has no "when" block on top: it never runs, and is drawn faint
        std::map<std::uint32_t, std::string> problems_;
        std::map<std::uint32_t, ImVec2> positions_; // canvas position of each block drawn this frame

        std::vector<StackTarget> stackTargets_;
        std::vector<SlotTarget> slotTargets_;
        std::vector<SlotTarget> inputTargets_;
        std::optional<Drag> drag_;
        std::optional<StackTarget> snapStack_;
        std::optional<SlotTarget> snapSlot_;
        std::optional<SlotTarget> snapInput_;
        // A block picked up this frame (taken out once the canvas is drawn)
        struct Pick
        {
            Place place;
            ImVec2 grab;
            bool fromPalette = false;
            Engine::Logic::Op op = Engine::Logic::Op::MoveForward;
            std::string text; // a palette block's field (a variable block's variable)
        };
        std::optional<Pick> pick_;
        // Duplicate / Delete from a block's menu (done once the canvas is drawn)
        struct MenuAction
        {
            enum class Kind
            {
                Duplicate,
                Delete,
                Unpack, // a library block: its blocks in its place
                DuplicateSelected,
                DeleteSelected
            };
            Place place;
            Kind kind = Kind::Delete;
        };
        std::optional<MenuAction> menuAction_;
        bool cleanUp_ = false;
        bool shadow_ = false; // Ghost mode draws the drop preview: a grey shadow where the blocks would go

        // Make a Variable / Rename popups
        std::array<char, 33> variableName_{};
        float variableValue_ = 0.0f;
        // The new variable is shown as true / false
        bool variableTrueFalse_ = false;
        std::string renaming_; // the variable the Rename popup is for

        // The block library, and Save to Library / Rename for it
        Engine::Logic::Library* library_ = nullptr;
        std::function<void(std::string_view, std::string_view)> onLibraryChanged_;
        std::uint32_t saveBlockId_ = 0; // the block Save to Library was picked on (its stack is saved from it down)
        bool openSave_ = false;
        std::array<char, 49> libraryName_{};
        std::string renamingLibrary_;
        std::array<char, 128> libraryInputs_{};
        std::string inputsOfLibrary_;
        std::string libraryMenuFor_; // the My Block its right-click menu is for
        bool openLibraryMenu_ = false;
        bool openLibraryInputs_ = false;
        bool openLibraryRename_ = false;
        bool openRename_ = false;

        // The library block open in its window: a stand-in object whose one script is its blocks
        struct LibraryEdit
        {
            std::string name;
            std::unique_ptr<Engine::Scene::GameObject> object;
            ImVec2 scroll{24.0f, 24.0f};
            // Its own undo, for as long as it is open: Cancel still throws all of it away
            Engine::History::UndoRedoManager history{};
            std::optional<Engine::Logic::LogicDesc> committed{};
        };
        std::optional<LibraryEdit> libraryEdit_;
        std::string openLibraryEdit_; // asked for this frame (a double-click or Open), opened once the canvas is drawn
        bool openEditPopup_ = false;

        std::uint32_t revealId_ = 0;
        bool revealPending_ = false;
        bool revealOnlyIfHidden_ = false;
        double revealTime_ = -10.0;
    };
}
