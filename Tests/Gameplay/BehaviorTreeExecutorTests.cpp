#include <doctest/doctest.h>

#include "AI/UBehaviorTree.hpp"
#include "AI/UBehaviorTreeComponent.hpp"

namespace Leon {

    TEST_SUITE("BehaviorTreeExecutor") {

        TEST_CASE("sequence remembers child index across InProgress ticks") {
            auto waitA = CreateRef<UBTTask_Wait>(0.05f);
            auto waitB = CreateRef<UBTTask_Wait>(0.20f);
            auto seq = CreateRef<UBTComposite_Sequence>("Seq");
            seq->AddChild(waitA);
            seq->AddChild(waitB);

            auto tree = CreateRef<UBehaviorTree>();
            tree->SetRoot(seq);

            UBehaviorTreeComponent brain("Brain");
            brain.StartTree(tree);

            brain.Tick(0.02f);
            CHECK(brain.GetActiveTask() != nullptr);
            CHECK(seq->ActiveChildIndex == 0);

            brain.Tick(0.04f); // finish waitA; same tick starts waitB (still InProgress)
            CHECK(seq->ActiveChildIndex == 1);
            CHECK(brain.GetActiveTask() != nullptr);

            brain.Tick(0.02f); // still on waitB
            CHECK(seq->ActiveChildIndex == 1);
            CHECK(brain.GetActiveTask() != nullptr);
        }

        TEST_CASE("selector does not restart earlier sibling while child InProgress") {
            auto fail =
                CreateRef<UBTTask_Native>("Fail", [](UBehaviorTreeComponent&, float) { return EBTNodeResult::Failed; });
            auto wait = CreateRef<UBTTask_Wait>(0.1f);
            auto sel = CreateRef<UBTComposite_Selector>("Sel");
            sel->AddChild(fail);
            sel->AddChild(wait);

            auto tree = CreateRef<UBehaviorTree>();
            tree->SetRoot(sel);

            UBehaviorTreeComponent brain("Brain");
            brain.StartTree(tree);

            brain.Tick(0.02f);
            CHECK(sel->ActiveChildIndex == 1);
            CHECK(brain.GetActiveTask() == wait.get());

            brain.Tick(0.02f);
            CHECK(sel->ActiveChildIndex == 1);
            CHECK(brain.GetActiveTask() == wait.get());
        }
    }

} // namespace Leon
