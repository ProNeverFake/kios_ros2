#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "rcl_interfaces/msg/parameter.hpp"
#include "rcl_interfaces/srv/get_parameters.hpp"
#include "rcl_interfaces/srv/set_parameters_atomically.hpp"

#include "kios_interface/msg/mios_state.hpp"
#include "kios_interface/msg/sensor_state.hpp"
#include "kios_interface/msg/task_state.hpp"

#include "kios_utils/kios_utils.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

using std::placeholders::_1;

class Messenger : public rclcpp::Node
{
public:
    Messenger()
        : Node("messenger")

    {
        // * set ros2 logger severity level
        auto logger = this->get_logger();
        rcutils_logging_set_logger_level(logger.get_name(), RCUTILS_LOG_SEVERITY_WARN);

        this->declare_parameter("power", true);

        timer_callback_group_ = this->create_callback_group(
            rclcpp::CallbackGroupType::MutuallyExclusive);
        subscription_callback_group_ = timer_callback_group_;
        publisher_callback_group_ = timer_callback_group_;

        //* initialize timer
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&Messenger::timer_callback, this), timer_callback_group_);

        //* initialize ros pub sub options
        rclcpp::QoS qos(rclcpp::QoSInitialization::from_rmw(rmw_qos_profile_sensor_data));
        rclcpp::SubscriptionOptions subscription_options;
        subscription_options.callback_group = subscription_callback_group_;
        rclcpp::PublisherOptions publisher_options;
        publisher_options.callback_group = publisher_callback_group_;

        //* initialize the pub sub callbacks
        mios_state_subscription_ = this->create_subscription<kios_interface::msg::MiosState>(
            "mios_state_topic",
            qos,
            std::bind(&Messenger::mios_state_subscription_callback, this, _1),
            subscription_options);
        sensor_state_subscription_ = this->create_subscription<kios_interface::msg::SensorState>(
            "sensor_state_topic",
            qos,
            std::bind(&Messenger::sensor_state_subscription_callback, this, _1),
            subscription_options);

        task_state_publisher_ = this->create_publisher<kios_interface::msg::TaskState>(
            "task_state_topic",
            qos,
            publisher_options);

        rclcpp::sleep_for(std::chrono::seconds(3));
    }

    bool check_power()
    {
        return this->get_parameter("power").as_bool();
    }

private:
    kios_interface::msg::TaskState task_state_msg_;

    // callback group
    rclcpp::CallbackGroup::SharedPtr publisher_callback_group_;
    rclcpp::CallbackGroup::SharedPtr subscription_callback_group_;
    rclcpp::CallbackGroup::SharedPtr timer_callback_group_;

    // callbacks
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<kios_interface::msg::TaskState>::SharedPtr task_state_publisher_;
    rclcpp::Subscription<kios_interface::msg::MiosState>::SharedPtr mios_state_subscription_;
    rclcpp::Subscription<kios_interface::msg::SensorState>::SharedPtr sensor_state_subscription_;

    void mios_state_subscription_callback(kios_interface::msg::MiosState::SharedPtr msg)
    {
        if (check_power() == true)
        {
            RCLCPP_DEBUG(this->get_logger(), "MIOS SUB hit.");
            task_state_msg_.mios_state = std::move(*msg);
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "POWER OFF, SUBSCRIPTION PASS ...");
        }
    }
    void sensor_state_subscription_callback(kios_interface::msg::SensorState::SharedPtr msg)
    {
        if (check_power() == true)
        {
            RCLCPP_INFO(this->get_logger(), "SENSOR SUB hit.");
            task_state_msg_.sensor_state = std::move(*msg);
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "POWER OFF, SUBSCRIPTION PASS ...");
        }
    }

    void timer_callback()
    {
        if (check_power() == true)
        {
            RCLCPP_INFO(this->get_logger(), "Publishing task_state.");
            task_state_publisher_->publish(task_state_msg_);
        }
        else
        {
            RCLCPP_ERROR(this->get_logger(), "POWER OFF, TIMER PASS ...");
        }
    }
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    auto messenger = std::make_shared<Messenger>();
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(messenger);

    executor.spin();

    rclcpp::shutdown();
    return 0;
}