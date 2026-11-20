// Block logic and the event bus: Broadcast blocks are sent on the bus, and messages from engine code reach the
// blocks' "when I receive" scripts (LogicRunner::SetEventBus, LogicRunner::Receive)

#include "engine/game/TimerSystem.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/messenger/EventBus.hpp"
#include "engine/messenger/Events.hpp"
#include "engine/scene/Scene.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Engine::Core::EventBus;
using Engine::Core::Subscription;
using Engine::Logic::Block;
using Engine::Logic::LogicDesc;
using Engine::Logic::LogicRunner;
using Engine::Logic::MakeBlock;
using Engine::Logic::Op;
using Engine::Logic::Script;
namespace Events = Engine::Events;

namespace
{
    Block TextBlock(Op op, std::uint32_t id, const char* text)
    {
        Block block = MakeBlock(op, id);
        block.text = text;
        return block;
    }

    Block MoveRight(std::uint32_t id)
    {
        Block block = MakeBlock(Op::MoveBy, id);
        block.numbers[0] = 1.0f;
        return block;
    }

    LogicDesc OneScript(std::vector<Block> blocks)
    {
        return LogicDesc{.scripts = {Script{.blocks = std::move(blocks)}}};
    }

    // An ECS world with a sender and a receiver, a runner for their logic, and a bus
    class BlockEventTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            sender_ = ecs_.CreateEntity();
            receiver_ = ecs_.CreateEntity();
            runner_.SetEventBus(&bus_);
        }

        void Update() { runner_.Update(ecs_, 1.0 / 60.0, LogicRunner::Keys{}); }

        float ReceiverX() const { return ecs_.GetTransform(receiver_)->x; }

        EventBus bus_;
        Engine::Scene::Scene ecs_;
        Engine::Scene::Entity sender_;
        Engine::Scene::Entity receiver_;
        LogicRunner runner_;
    };
}

// A Broadcast block also goes on the bus, with its message and the object that sent it, at the next frame's
// DispatchQueued (queued, so no handler runs while the runner is walking its objects)
TEST_F(BlockEventTest, BroadcastBlockIsSentOnTheBus)
{
    std::vector<Events::MessageBroadcast> heard;
    const Subscription sub = bus_.Subscribe<Events::MessageBroadcast>(
        [&](const Events::MessageBroadcast& e) { heard.push_back(e); });
    runner_.Set(7, "Door", sender_.GetId(),
                OneScript({MakeBlock(Op::WhenPlayStarts, 1), TextBlock(Op::Broadcast, 2, "open")}));

    Update();
    EXPECT_TRUE(heard.empty()) << "queued, not sent during the update";
    bus_.DispatchQueued();

    ASSERT_EQ(heard.size(), 1u);
    EXPECT_EQ(heard[0].message, "open");
    EXPECT_EQ(heard[0].senderObjectId, 7u);
    EXPECT_TRUE(heard[0].fromBlocks);
}

// A message from engine code (Receive) starts the "when I receive" scripts for it on the next update, as a
// Broadcast block does (messages ignore case)
TEST_F(BlockEventTest, MessageFromEngineCodeStartsReceivers)
{
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenIReceive, 1, "Open Door"), MoveRight(2)}));

    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 0.0f) << "nothing sent yet";
    runner_.Receive("open door");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f) << "once per message";
}

// Wired as GameRuntime wires it (engine messages on the bus passed to Receive, the blocks' own skipped), a
// Broadcast block's message is heard once, not twice
TEST_F(BlockEventTest, BlocksOwnBroadcastIsHeardOnce)
{
    const Subscription forward = bus_.Subscribe<Events::MessageBroadcast>(
        [&](const Events::MessageBroadcast& e)
        {
            if (!e.fromBlocks)
            {
                runner_.Receive(e.message);
            }
        });
    runner_.Set(1, "Sender", sender_.GetId(),
                OneScript({MakeBlock(Op::WhenPlayStarts, 1), TextBlock(Op::Broadcast, 2, "go")}));
    runner_.Set(2, "Receiver", receiver_.GetId(), OneScript({TextBlock(Op::WhenIReceive, 1, "go"), MoveRight(2)}));

    Update(); // sends
    bus_.DispatchQueued();
    Update(); // heard
    bus_.DispatchQueued();
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
}

// An engine message sent on the bus, wired the same way, reaches the blocks
TEST_F(BlockEventTest, EngineMessageOnTheBusReachesTheBlocks)
{
    const Subscription forward = bus_.Subscribe<Events::MessageBroadcast>(
        [&](const Events::MessageBroadcast& e)
        {
            if (!e.fromBlocks)
            {
                runner_.Receive(e.message);
            }
        });
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenIReceive, 1, "level done"), MoveRight(2)}));

    bus_.Emit(Events::MessageBroadcast{.message = "level done"});
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
}

// Without a bus the blocks work as before, and Receive still reaches them
TEST(BlockEvents, RunnerWorksWithoutABus)
{
    Engine::Scene::Scene ecs;
    const Engine::Scene::Entity entity = ecs.CreateEntity();
    LogicRunner runner;
    runner.Set(1, "Cube", entity.GetId(), OneScript({TextBlock(Op::WhenIReceive, 1, "go"), MoveRight(2)}));

    runner.Receive("go");
    runner.Update(ecs, 1.0 / 60.0, LogicRunner::Keys{});
    EXPECT_FLOAT_EQ(ecs.GetTransform(entity)->x, 1.0f);
}

// ---- Touching and timers (the "when I touch" / "when timer ends" hats, the "start timer" block) ----

// Touched starts the object's "when I touch" scripts for that object (names ignore case), not for another one
TEST_F(BlockEventTest, TouchStartsWhenITouchForThatObject)
{
    runner_.Set(2, "Receiver", receiver_.GetId(), OneScript({TextBlock(Op::WhenITouch, 1, "Coin"), MoveRight(2)}));

    runner_.Touched(2, "Wall");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 0.0f) << "a different object";
    runner_.Touched(2, "coin");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
    runner_.Touched(3, "Coin");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f) << "another object touched it, not this one";
}

// "when I touch anything" starts for any object touched
TEST_F(BlockEventTest, WhenITouchAnything)
{
    runner_.Set(2, "Receiver", receiver_.GetId(), OneScript({TextBlock(Op::WhenITouch, 1, "anything"), MoveRight(2)}));

    runner_.Touched(2, "Wall");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
}

// TimerEnded starts the "when timer ... ends" scripts for that timer only
TEST_F(BlockEventTest, TimerEndedStartsItsScripts)
{
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenTimerEnds, 1, "Alarm"), MoveRight(2)}));

    runner_.TimerEnded("other");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 0.0f);
    runner_.TimerEnded("alarm");
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
}

// "start timer" sends TimerStarted with its name, its seconds and the object whose block it was
TEST_F(BlockEventTest, StartTimerBlockSendsTimerStarted)
{
    std::vector<Events::TimerStarted> started;
    const Subscription sub = bus_.Subscribe<Events::TimerStarted>(
        [&](const Events::TimerStarted& e) { started.push_back(e); });
    Block start = TextBlock(Op::StartTimer, 2, "door");
    start.numbers[0] = 2.5f;
    runner_.Set(7, "Button", sender_.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), start}));

    Update();
    bus_.DispatchQueued();

    ASSERT_EQ(started.size(), 1u);
    EXPECT_EQ(started[0].name, "door");
    EXPECT_DOUBLE_EQ(started[0].seconds, 2.5);
    EXPECT_EQ(started[0].objectId, 7u);
}

// The whole loop, wired as GameRuntime wires it: "start timer alarm for 1 seconds" -> the TimerSystem counts it down
// -> TimerElapsed -> the "when timer alarm ends" script runs
TEST_F(BlockEventTest, StartTimerToWhenTimerEnds)
{
    Engine::Game::TimerSystem timers;
    timers.Connect(&bus_);
    const Subscription toBlocks = bus_.Subscribe<Events::TimerElapsed>(
        [&](const Events::TimerElapsed& e) { runner_.TimerEnded(e.name); });
    Block start = TextBlock(Op::StartTimer, 2, "alarm");
    start.numbers[0] = 1.0f;
    runner_.Set(1, "Sender", sender_.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), start}));
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenTimerEnds, 1, "alarm"), MoveRight(2)}));

    Update();              // the block starts the timer
    bus_.DispatchQueued(); // the TimerSystem hears it
    timers.Update(0.5);
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 0.0f) << "not yet";
    timers.Update(0.6); // runs out: TimerElapsed
    Update();
    EXPECT_FLOAT_EQ(ReceiverX(), 1.0f);
}

// Without the game's event bus, "start timer" says so instead of doing nothing quietly
TEST(BlockEvents, StartTimerWithoutABusIsAProblem)
{
    Engine::Scene::Scene ecs;
    const Engine::Scene::Entity entity = ecs.CreateEntity();
    LogicRunner runner;
    runner.Set(1, "Cube", entity.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), MakeBlock(Op::StartTimer, 2)}));

    runner.Update(ecs, 1.0 / 60.0, LogicRunner::Keys{});

    ASSERT_EQ(runner.Problems(1).size(), 1u);
    EXPECT_EQ(runner.Problems(1)[0].block, 2u);
}

// ---- Messages with a number ("broadcast ... with ...", "message value") ----

namespace
{
    // "change position by x [message value]"
    Block MoveRightByMessageValue(std::uint32_t id)
    {
        Block block = MakeBlock(Op::MoveBy, id);
        block.inputs[0] = MakeBlock(Op::MessageValue, id + 100);
        return block;
    }
}

// "broadcast score with 5": the receiver reads 5 as "message value", and the bus gets the number too
TEST_F(BlockEventTest, BroadcastWithValueReachesTheReceiver)
{
    std::vector<Events::MessageBroadcast> heard;
    const Subscription sub = bus_.Subscribe<Events::MessageBroadcast>(
        [&](const Events::MessageBroadcast& e) { heard.push_back(e); });
    Block send = TextBlock(Op::BroadcastWithValue, 2, "score");
    send.numbers[0] = 5.0f;
    runner_.Set(1, "Sender", sender_.GetId(), OneScript({MakeBlock(Op::WhenPlayStarts, 1), send}));
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenIReceive, 1, "Score"), MoveRightByMessageValue(2)}));

    Update(); // sends
    bus_.DispatchQueued();
    Update(); // heard

    EXPECT_FLOAT_EQ(ReceiverX(), 5.0f);
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_DOUBLE_EQ(heard[0].value, 5.0);
}

// A message from engine code can carry a number too
TEST_F(BlockEventTest, EngineMessageWithValue)
{
    runner_.Set(2, "Receiver", receiver_.GetId(),
                OneScript({TextBlock(Op::WhenIReceive, 1, "go"), MoveRightByMessageValue(2)}));

    runner_.Receive("go", 3.0);
    Update();

    EXPECT_FLOAT_EQ(ReceiverX(), 3.0f);
}

// A plain broadcast has no number: "message value" is 0
TEST_F(BlockEventTest, PlainBroadcastHasValueZero)
{
    Block hop = MoveRightByMessageValue(2);
    hop.numbers[0] = 9.0f; // the typed number is not used: the value block is
    runner_.Set(1, "Sender", sender_.GetId(),
                OneScript({MakeBlock(Op::WhenPlayStarts, 1), TextBlock(Op::Broadcast, 2, "go")}));
    runner_.Set(2, "Receiver", receiver_.GetId(), OneScript({TextBlock(Op::WhenIReceive, 1, "go"), hop}));

    Update();
    Update();

    EXPECT_FLOAT_EQ(ReceiverX(), 0.0f);
}
