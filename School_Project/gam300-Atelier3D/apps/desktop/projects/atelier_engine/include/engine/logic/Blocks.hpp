/* !
@file    	    Blocks.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Declares the block language: the blocks a game object's logic is built
    from (Scratch-style hats, stack blocks, C blocks, conditions and values),
    its variables, the table that describes each kind of block, and the rules
    for where a block may be placed.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Logic
{
    // What a block does. The order is the palette's order within each category (the scene file uses the names in the
    // block table, so the order can change).
    enum class Op : std::uint8_t
    {
        // Events: hats that start a script, and messages between objects
        WhenPlayStarts,
        WhenKeyPressed,
        WhenIReceive,
        Broadcast,
        // Motion
        MoveForward,
        MoveRight,
        TurnBy,
        GoTo,
        MoveBy,
        GlideTo,
        GoToObject,
        PointTowards,
        // Looks
        Show,
        Hide,
        SetColor,
        SetSize,
        ChangeSizeBy,
        Say,
        // Loops
        Repeat,
        RepeatUntil,
        Forever,
        Wait,
        WaitUntil,
        Stop,
        // Conditions: ifs, and the questions they ask
        If,
        IfElse,
        Equal,
        Less,
        Greater,
        And,
        Or,
        Not,
        // Math: values made of values
        Add,
        Subtract,
        Multiply,
        Divide,
        Mod,
        Random,
        Round,
        Abs,
        // Variables
        SetVariable,
        ChangeVariable,
        VariableValue,
        // Sensing
        KeyDown,
        Timer,
        ResetTimer,
        PositionX,
        PositionY,
        PositionZ,
        Direction,
        DistanceTo,
        // Physics: for objects with a moving (Dynamic) Rigid Body
        Jump,
        PushForward,
        PushBy,
        OnGround,
        Touching,
        // More Motion, Sensing and Events, then Screen: what the game shows on its screen
        PointInDirection,
        DirectionOf,
        GoToScene,
        ShowScreenText,
        ShowScreenPicture,
        WhenScreenClicked,
        LockMouse,
        FreeMouse,
        // Animation (in Looks): the clips of an object's model, played by its Animator
        PlayAnimation,
        PlayAnimationOnce,
        StopAnimation,
        SetAnimationSpeed,
        // Sky (in Looks): the scene's sky, its colours, picture and haze
        // Aiming up and down: tilt an object, and read how far one (a player: its camera) is tilted
        SetTilt,
        FlyForward,
        TiltOf,
        // Sound: one of the game's sounds, played once from where the object is
        PlaySound,
        PlayFootstep,
        // More Events, started through the event bus: touching another object, and countdown timers
        WhenITouch,
        WhenTimerEnds,
        StartTimer,
        BroadcastWithValue,
        MessageValue,
        // Shooting: a shot straight ahead (the player's along its view), what it hit, and being hit
        // The camera: how wide it sees (aiming in), and how high the player's eyes are (crouching)
        SetViewAngle,
        SetCameraHeight,
        // Game: the player's settings and the window
        SetSensitivity,
        SetVolume,
        SetFullscreen,
        SetWindowSize,
        QuitGame,
        SettingOf,
        SetSetting,
        // Rays: what is in a line, from which everything that shoots or looks is made
        RayAhead,
        RayAt,
        RayHit,
        RayDistance,
        TellRayHit,
        // Another object's variables, by "Object.variable" ("hit.variable": what the ray hit)
        ValueOf,
        SetValueOf,
        ChangeValueOf,
        ValueExists,
        //beginner events and actions
        WhenKeyReleased,
        WhenKeyHeld,
        WhenCollision,
        WhenTimer,
        MoveBack,
        ApplyForce,
        SetSpeed,
        // Library: a stack saved under a name (Library.hpp), run as one block
        Call,
    };

    // The palette's groups, each with its own colour
    enum class Category : std::uint8_t
    {
        Events,
        Motion,
        Looks,
        Loops,
        Conditions,
        Math,
        Variables,
        Sensing,
        Physics,
        Screen,
        Sound,
        Game,
        Library,
    };

    // A block's shape, which says where it fits:
    //   Hat       - starts a script: nothing above it (rounded top)
    //   Stack     - one under another (a notch on top, a bump below)
    //   Cap       - ends a script: nothing below it (Forever, Stop)
    //   Condition - a true/false question that fits a hexagonal slot (if, repeat until, and, ...)
    //   Value     - a number that fits a round slot: any number of a block can be one (variables, math, sensing)
    // A Stack or Cap block can also hold stacks of its own (Repeat, Forever, If): a C block. Conditions and values are
    // never in a stack; on the canvas they lie loose on their own until put in a slot.
    enum class BlockShape : std::uint8_t
    {
        Hat,
        Stack,
        Cap,
        Condition,
        Value,
    };

    // One block. Blocks hold the blocks inside them, so an object's logic is a tree: each script is a list of blocks,
    // each C block holds a list (body, and else for If-Else), each condition slot one condition block, and each number
    // can be replaced by a value block.
    struct Block
    {
        std::uint32_t id = 0; // unique within the object's logic: what the runtime reports and the editor highlights
        Op op = Op::MoveForward;
        std::array<float, 4> numbers{}; // its number fields, in label order (unused ones stay 0)
        std::string text;               // its key, message, object or variable name, if its label has one
        std::vector<Block> inputs;      // one per number field: a value block used instead of the number (id 0: none)
        std::vector<Block> slots;       // one per condition slot of its label; a slot with id 0 is empty
        std::vector<Block> body;        // inside a C block
        std::vector<Block> elseBody;    // inside If-Else's "else"

        bool operator==(const Block&) const = default;
    };

    // A stack of blocks where it lies on the block canvas. Only a stack that starts with a hat runs; a loose one is
    // kept as it is (being built, or put aside).
    struct Script
    {
        float x = 0.0f;
        float y = 0.0f;
        std::vector<Block> blocks;

        bool operator==(const Script&) const = default;
    };

    // How a variable's value is shown. Inside it is always a number; a true/false one reads 0 as false and anything
    // else as true.
    enum class VariableKind : std::uint8_t
    {
        Number,
        TrueFalse,
    };

    // A number the object's blocks can set, change and use, with the value it starts with each time Play starts
    struct Variable
    {
        std::string name;
        float value = 0.0f;
        VariableKind kind = VariableKind::Number;

        bool operator==(const Variable&) const = default;
    };

    // A game object's logic: the scripts of the block canvas and the object's variables
    struct LogicDesc
    {
        std::vector<Script> scripts;
        std::vector<Variable> variables;

        bool operator==(const LogicDesc&) const = default;
    };

    // ---- What blocks write

    // The object's values that blocks other than the variable blocks write
    enum class Property : std::uint8_t
    {
        Position, // move, go to, glide
        Rotation, // turn, point towards
        Size,     // set size, change size
        Visible,  // show, hide
        Colour,   // set colour
        Says,     // say
    };

    // Where a property is saved: the component and fields of the scene file's layout ("transform": "x", "y", "z").
    // Unused fields are empty, and so is the component of a property that is not saved (Visible and Says).
    struct PropertyInfo
    {
        Property property;
        std::string_view component;
        std::array<std::string_view, 3> fields;
    };
    // Every property, in the order of Property
    inline constexpr std::array kProperties{
        PropertyInfo{Property::Position, "transform", {"x", "y", "z"}},
        PropertyInfo{Property::Rotation, "transform", {"rotation_x", "rotation", "rotation_z"}},
        PropertyInfo{Property::Size, "transform", {"size", "", ""}},
        PropertyInfo{Property::Visible, "", {"", "", ""}},
        PropertyInfo{Property::Colour, "renderable", {"albedo_r", "albedo_g", "albedo_b"}},
        PropertyInfo{Property::Says, "", {"", "", ""}},
    };
    inline constexpr std::size_t kPropertyCount = kProperties.size();
    static_assert(kProperties.back().property == static_cast<Property>(kPropertyCount - 1),
                  "kProperties has one entry per Property, in order");

    // ---- The kinds of block

    // What the block table says about one kind of block. A label is words and fields:
    //   %n a number (or a value block), %k a key, %t text (a message, an object's name), %v a variable,
    //   %c a condition slot
    // e.g. "move forward %n" or "if %c then".
    struct BlockInfo
    {
        Op op;
        std::string_view name; // in the scene file, e.g. "move_forward"
        Category category;
        BlockShape shape;
        std::string_view label;
        bool hasBody = false; // a C block
        bool hasElse = false; // a second arm, "else"
        std::array<float, 4> numbers{};
        std::string_view text; // the default of its key or text field
        std::string_view help; // what it does, in a sentence (the palette's tooltip)
        // The value it writes when it runs, if any: move forward writes Position, show writes Visible, ... (the set
        // and change variable blocks write a variable instead, see WritesVariable)
        std::optional<Property> writes;
    };

    // Every kind of block, in palette order
    std::span<const BlockInfo> AllBlocks();
    const BlockInfo& InfoOf(Op op);
    std::optional<Op> OpNamed(std::string_view name);
    std::string_view CategoryName(Category category);
    // A sentence about what the category's blocks do (the palette shows it)
    std::string_view CategoryHelp(Category category);
    // The palette's colour of a category, as 0xRRGGBB
    std::uint32_t CategoryColor(Category category);
    constexpr std::array kCategories{Category::Events,     Category::Motion, Category::Looks,     Category::Loops,
                                     Category::Conditions, Category::Math,   Category::Variables, Category::Sensing,
                                     Category::Physics,    Category::Screen, Category::Sound,     Category::Game,
                                     Category::Library};

    // How many number fields and condition slots the label has, and its one text-like field, if any
    std::size_t NumberCount(Op op);
    std::size_t SlotCount(Op op);
    bool HasKeyField(Op op);
    bool HasTextField(Op op);
    bool HasVariableField(Op op);

    // A new block of this kind with the table's defaults (empty slots and inputs, empty body) and the given id
    Block MakeBlock(Op op, std::uint32_t id);

    // ---- Ids

    // The next id no block of logic uses (1 for none)
    std::uint32_t NextBlockId(const LogicDesc& logic);
    // Gives every block of blocks a new id from nextId on (for a copy, so ids stay unique)
    void Renumber(std::vector<Block>& blocks, std::uint32_t& nextId);
    // The block with this id, or nullptr
    const Block* FindBlock(const LogicDesc& logic, std::uint32_t id);
    // The block with this id in blocks (inside others too), or nullptr
    const Block* FindBlock(const std::vector<Block>& blocks, std::uint32_t id);
    // Whether blocks (or the blocks inside them) have a block with one of the ids, e.g. a script with a running block
    bool ContainsAny(const std::vector<Block>& blocks, const std::vector<std::uint32_t>& ids);
    // How many blocks logic has, counting those inside others
    std::size_t CountBlocks(const LogicDesc& logic);
    // Removes the blocks with these ids, each as Delete Block does: what is inside it goes with it, what is under it
    // stays, and a value in a slot leaves the slot empty. Scripts left with no blocks go too. Returns how many were
    // named and removed (a block inside a removed one is not counted).
    std::size_t RemoveBlocks(LogicDesc& logic, const std::set<std::uint32_t>& ids);
    // Copies of the stack blocks with these ids, keeping their ids (Renumber them to use them): each block with what
    // is inside it, and neighbours in a stack kept together as one list, in script order. A block inside a chosen block
    // comes with it, and values in slots are left out.
    std::vector<std::vector<Block>> CopyBlocks(const LogicDesc& logic, const std::set<std::uint32_t>& ids);

    // ---- Variables

    // The variable called name, or nullptr
    const Variable* FindVariable(const LogicDesc& logic, std::string_view name);
    // Whether name can be a new variable: 1-32 characters, not blank, not already used
    bool CanAddVariable(const LogicDesc& logic, std::string_view name);
    // Renames a variable and every block that uses it. Returns false if there is no such variable or the new name
    // cannot be used.
    bool RenameVariable(LogicDesc& logic, std::string_view from, std::string_view to);
    // How many blocks use the variable
    std::size_t CountUses(const LogicDesc& logic, std::string_view name);

    // ---- Who writes what

    // The value a block of this kind writes, if any (its table row's writes)
    std::optional<Property> PropertyOf(Op op);
    // Whether a block of this kind sets or changes a variable (a stack block with a variable field)
    bool WritesVariable(Op op);
    // The property a saved field belongs to, by the scene file's names ("transform", "x" is Position); nullopt for a
    // field no block writes. Visible and Says are not saved, so no field has them.
    std::optional<Property> PropertyOfField(std::string_view component, std::string_view field);

    // A block found in an object's logic, with the script it is in and whether that script runs (starts with a hat)
    struct BlockAt
    {
        const Block* block = nullptr;
        std::size_t script = 0;
        bool runs = false;
    };
    // The set / change blocks of the variable, in script order
    std::vector<BlockAt> WritersOf(const LogicDesc& logic, std::string_view variable);
    // The blocks that write the property, in script order
    std::vector<BlockAt> WritersOf(const LogicDesc& logic, Property property);

    // ---- Where blocks fit (illegal connections are refused while dragging)

    // Whether stack (a dragged block and those below it) may be put into list before index. topLevel is true when
    // list is a script's own list, false inside a C block. A hat only starts a script, nothing goes under a cap, a cap
    // only goes last, and conditions and values only go into slots.
    bool CanInsert(const std::vector<Block>& list, std::size_t index, const std::vector<Block>& stack, bool topLevel);
    // Whether block may be put into slot: an empty condition slot takes one condition
    bool CanFillSlot(const Block& slot, const Block& block);
    // Whether block may be put into input: an empty number field takes one value block
    bool CanFillInput(const Block& input, const Block& block);
    // Removes the block at index of list and every block below it, and returns them (Scratch drags the rest of a stack
    // along with the block it is picked up by)
    std::vector<Block> TakeStack(std::vector<Block>& list, std::size_t index);

    // ---- Words

    // A number as people write it: 5, 2.5, 0.333 (at most 3 decimals), 0 rather than -0, and infinity or
    // "not a number" in words
    std::string FormatNumber(double value);
    // A variable's value as its kind shows it: true / false, or the number
    std::string ValueText(VariableKind kind, double value);
    // The block as a sentence with its values, e.g. "move forward 2", "if (key Space pressed?) then" or
    // "turn (pick random -30 to 30) degrees"
    std::string Describe(const Block& block);
}
