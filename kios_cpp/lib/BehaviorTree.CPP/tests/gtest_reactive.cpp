#include <gtest/gtest.h>
#include "behaviortree_cpp/bt_factory.h"
#include "test_helper.hpp"

using BT::NodeStatus;
using std::chrono::milliseconds;

TEST(Reactive, RunningChildren)
{
  static const char* reactive_xml_text = R"(
<root BTCPP_format="4" >
  <BehaviorTree ID="MainTree">
    <ReactiveSequence>
      <Sequence name="first">
        <TestA/>
        <TestB/>
        <TestC/>
      </Sequence>
      <AsyncSequence name="second">
        <TestD/>
        <TestE/>
        <TestF/>
      </AsyncSequence>
    </ReactiveSequence>
  </BehaviorTree>
</root>
)";

  BT::BehaviorTreeFactory factory;
  std::array<int, 6> counters;
  RegisterTestTick(factory, "Test", counters);

  auto tree = factory.createTreeFromText(reactive_xml_text);

  NodeStatus status = NodeStatus::IDLE;

  int count=0;
  while(!BT::isStatusCompleted(status) && count<100)
  {
    count++;
    status = tree.tickExactlyOnce();
  }

  ASSERT_NE(100, count);

  ASSERT_EQ(status, NodeStatus::SUCCESS);

  ASSERT_EQ(counters[0], 3);
  ASSERT_EQ(counters[1], 3);
  ASSERT_EQ(counters[2], 3);

  ASSERT_EQ(counters[3], 1);
  ASSERT_EQ(counters[4], 1);
  ASSERT_EQ(counters[5], 1);
}


TEST(Reactive, Issue587)
{
  // TestA should be executed only once, because of the variable "test"

  static const char* reactive_xml_text = R"(
<root BTCPP_format="4" >
  <BehaviorTree ID="Example A">
    <Sequence>
      <Script code="test := false"/>
      <ReactiveSequence>
        <RetryUntilSuccessful name="Retry 1" num_attempts="-1" _skipIf="test ">
          <TestA name="Success 1" _onSuccess="test = true"/>
        </RetryUntilSuccessful>
        <RetryUntilSuccessful name="Retry 2" num_attempts="5">
          <AlwaysFailure name="Failure 2"/>
        </RetryUntilSuccessful>
      </ReactiveSequence>
    </Sequence>
  </BehaviorTree>
</root>
)";

  BT::BehaviorTreeFactory factory;
  std::array<int, 2> counters;
  RegisterTestTick(factory, "Test", counters);

  auto tree = factory.createTreeFromText(reactive_xml_text);
  tree.tickWhileRunning();

  ASSERT_EQ(counters[0], 1);
}


