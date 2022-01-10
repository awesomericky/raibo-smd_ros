#include <chrono>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/clock.hpp>

#include <tf2/exceptions.h>
#include <tf2/time.h>
#include <tf2/transform_datatypes.h>
#include <tf2_sensor_msgs/tf2_sensor_msgs.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_eigen/tf2_eigen.h>

#include <pcl_ros/transforms.hpp>
#include <pcl_conversions/pcl_conversions.h>

#include "std_msgs/msg/string.h"
#include "sensor_msgs/msg/point_cloud2.hpp"

using std::placeholders::_1;

class raibo_bridge : public rclcpp::Node{
public:

    raibo_bridge(): Node("raibo_bridge"){
        rclcpp::Time start_time = this->get_clock()->now();
        const rclcpp::Duration timeout(10.0);
        const rclcpp::Time     time(0.0);

        std::string target_frame = "base";
        std::string source_depthR = "d435i_R_depth_optical_frame";
        std::string source_depthL = "d435i_L_depth_optical_frame";
        // std::string source_lidar  = "velodyne";
        // this->declare_parameter("target_frame",target_frame);
        tf_buffer_   = std::make_unique<tf2_ros::Buffer>(this->get_clock());
        tf_buffer_ -> setUsingDedicatedThread(true);

        get_tf_eigen4(source_depthR,target_frame,depthR_stamped,depthR_eigen4);
        get_tf_eigen4(source_depthL,target_frame,depthL_stamped,depthL_eigen4);
        // get_tf_eigen4(source_lidar ,target_frame, lidar_stamped, lidar_eigen4);

        // std::string source_frame = "d435i_L_depth_optical_frame"
        // tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
        // std::string warning_msg;
        // while (rclcpp::ok() && !tf_buffer_->canTransform(
        //     target_frame, source_frame, tf2::TimePoint(), &warning_msg))
        // {
        //     RCLCPP_INFO(
        //         this->get_logger(),"Waiting for transform %s ->  %s:\n%s\nAvailable:\n%s",
        //         source_frame.c_str(), target_frame.c_str(),warning_msg.c_str(),(tf_buffer_->allFramesAsString()).c_str());
        //     rate.sleep();
        // }
        // tf_stamped = tf_buffer_-> lookupTransform(target_frame,source_frame,tf2::TimePoint());
        // tf_eigen4  = tf2::transformToEigen(tf_stamped.transform).matrix().cast<float>();
        // if(tf_stamped.transform.translation.x != 0.0){
        //     auto trn = tf_stamped.transform.translation;
        //     auto rot = tf_stamped.transform.rotation;
        //     RCLCPP_INFO(this->get_logger(),"Transformations successfully retreived.");
        //     RCLCPP_INFO(this->get_logger(),"[{%s} ->  {%s}]",source_frame.c_str(),target_frame.c_str());
        //     RCLCPP_INFO(this->get_logger(),"\tTRN-XYZ :[%.3f,%.3f,%.3f]"      ,trn.x,trn.y,trn.z);
        //     RCLCPP_INFO(this->get_logger(),"\tROT-XYZW:[%.3f,%.3f,%.3f,%.3f]" ,rot.x,rot.y,rot.z,rot.w);
        // }

        subscription_depthR_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/d435i_R/depth/color/points",10,std::bind(&raibo_bridge::depthR_callback,this,std::placeholders::_1)
        );
        publisher_    = this->create_publisher<sensor_msgs::msg::PointCloud2>("raibo_pcl2",3);
    }

private:
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscription_depthR_;
    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;

    geometry_msgs::msg::TransformStamped depthR_stamped;
    geometry_msgs::msg::TransformStamped depthL_stamped;
    geometry_msgs::msg::TransformStamped lidar_stamped;
    Eigen::Matrix4f depthL_eigen4;
    Eigen::Matrix4f depthR_eigen4;
    Eigen::Matrix4f lidar_eigen4;

    sensor_msgs::msg::PointCloud2 depthR_data;
    sensor_msgs::msg::PointCloud2 depthL_data;
    sensor_msgs::msg::PointCloud2  lidar_data;

    void get_tf_eigen4(std::string source_frame , std::string target_frame ,geometry_msgs::msg::TransformStamped& tf_stamped, Eigen::Matrix4f& tf_eigen4){
        std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
        std::string warning_msg;
        rclcpp::Rate rate(1.0);
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
        while (rclcpp::ok() && !tf_buffer_->canTransform(target_frame, source_frame, tf2::TimePoint(), &warning_msg)){
            RCLCPP_INFO(
                this->get_logger(),"Waiting for transform %s ->  %s:\n%s\nAvailable:\n%s",
                source_frame.c_str(), target_frame.c_str(),warning_msg.c_str(),(tf_buffer_->allFramesAsString()).c_str());
            rate.sleep();
        }

        tf_stamped = tf_buffer_-> lookupTransform(target_frame,source_frame,tf2::TimePoint());
        tf_eigen4  = tf2::transformToEigen(tf_stamped.transform).matrix().cast<float>();

        if(tf_stamped.transform.translation.x != 0.0){
            auto trn = tf_stamped.transform.translation;
            auto rot = tf_stamped.transform.rotation;
            RCLCPP_INFO(this->get_logger(),"Transformations successfully retreived.");
            RCLCPP_INFO(this->get_logger(),"[{%s} ->  {%s}]",source_frame.c_str(),target_frame.c_str());
            RCLCPP_INFO(this->get_logger(),"\tTRN-XYZ :[%.3f,%.3f,%.3f]"      ,trn.x,trn.y,trn.z);
            RCLCPP_INFO(this->get_logger(),"\tROT-XYZW:[%.3f,%.3f,%.3f,%.3f]" ,rot.x,rot.y,rot.z,rot.w);
        }
    }

    void depthR_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg) const {
        auto data = sensor_msgs::msg::PointCloud2();
        const auto src  = sensor_msgs::msg::PointCloud2();

        // data = *msg;
        // tf2::doTransform(*msg,data,this->tf_stamped);
        pcl_ros::transformPointCloud(depthR_eigen4,*msg,data);
        data.header.frame_id = std::string("base");
        publisher_ -> publish(data);
    }

    // void depthL_callback(const sensor_msgs:: //TODO: BOOKMARK

};


int main(int argc, char * argv[]){
    rclcpp::init(argc,argv);
    rclcpp::spin(std::make_shared<raibo_bridge>() );
    rclcpp::shutdown();
    return 0;
}