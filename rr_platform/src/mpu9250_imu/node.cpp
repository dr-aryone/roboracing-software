#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/MagneticField.h>
#include <boost/asio.hpp>
#include <boost/regex.hpp>

ros::Publisher imu_pub;
ros::Publisher mag_pub;

/**
 * @note http://stackoverflow.com/a/27511119
 */
std::vector <std::string> split(const std::string &s, char delim) {
    std::stringstream ss(s);
    std::string item;
    std::vector <std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(std::move(item));
    }
    return elems;
}

void parse_message(const std::string &line_in, sensor_msgs::Imu &imu_msg, sensor_msgs::MagneticField &mag_msg, bool &imu_ready, bool &mag_ready) {
    const auto tokens = split(line_in, ' ');
    imu_ready = false;
    mag_ready = false;

    if(tokens.size() < 9) {
       return;
    }

    if(tokens[0] == "ax") {
        imu_msg.linear_acceleration.x = std::atof(tokens[2].c_str());
        imu_msg.linear_acceleration.y = std::atof(tokens[5].c_str());
        imu_msg.linear_acceleration.z = std::atof(tokens[8].c_str());
    }
    else if(tokens[0] == "gx") {
        imu_msg.angular_velocity.x = std::atof(tokens[2].c_str());
        imu_msg.angular_velocity.y = std::atof(tokens[5].c_str());
        imu_msg.angular_velocity.z = std::atof(tokens[8].c_str());
    }
    else if(tokens[0] == "q0") {
        imu_msg.orientation.x = std::atof(tokens[2].c_str());
        imu_msg.orientation.y = std::atof(tokens[5].c_str());
        imu_msg.orientation.z = std::atof(tokens[8].c_str());
        imu_msg.orientation.w = std::atof(tokens[11].c_str());
        imu_ready = true;
    }
    else if(tokens[0] == "mx") {
        mag_msg.magnetic_field.x = std::atof(tokens[2].c_str());
        mag_msg.magnetic_field.y = std::atof(tokens[5].c_str());
        mag_msg.magnetic_field.z = std::atof(tokens[8].c_str());
        mag_ready = true;
    }

}

std::string readLine(boost::asio::serial_port &port) {
    std::string line = "";
    while (true) {
        char in;
        try {
            boost::asio::read(port, boost::asio::buffer(&in, 1));
        } catch (
                boost::exception_detail::clone_impl <boost::exception_detail::error_info_injector<boost::system::system_error>> &err) {
            ROS_ERROR("Error reading serial port.");
            ROS_ERROR_STREAM(err.what());
            return line;
        }
        if (in == '\n')
            return line;
        if (in == '\r')
            return line;
        line += in;
    }
}

int main(int argc, char **argv) {

    ros::init(argc, argv, "imu");

    ros::NodeHandle handle;
    ros::NodeHandle private_handle("~");

    imu_pub = handle.advertise<sensor_msgs::Imu>("/imu/data_raw", 1);
    mag_pub = handle.advertise<sensor_msgs::MagneticField>("/imu/mag", 1);

    // Serial port setup
    std::string serial_port_name;
    private_handle.param(std::string("serial_port"), serial_port_name, std::string("/dev/ttyUSB1"));
    boost::asio::io_service io_service;
    boost::asio::serial_port serial(io_service, serial_port_name);
    serial.set_option(boost::asio::serial_port_base::baud_rate(38400));

    if(!serial.is_open()) {
        ROS_FATAL_STREAM("Unable to open serial port: " << serial_port_name);
        return 1;
    }

    sensor_msgs::Imu imu_msg;
    sensor_msgs::MagneticField mag_msg;

    imu_msg.header.frame_id = "imu";
    mag_msg.header.frame_id = "mag";

    bool imu_ready = false;
    bool mag_ready = false;

    ROS_INFO("IMU Ready.");

    ros::Duration(2.0).sleep();

    while(ros::ok() && serial.is_open()) {
        ros::spinOnce();

        auto line_in = readLine(serial);

        if(!line_in.empty()) {
            parse_message(line_in, imu_msg, mag_msg, imu_ready, mag_ready);
            if(imu_ready) {
                sensor_msgs::Imu publishable_copy = imu_msg;
                publishable_copy.header.stamp = ros::Time::now();
                imu_pub.publish(publishable_copy);
            }
            if(mag_ready) {
                sensor_msgs::MagneticField publishable_copy = mag_msg;
                publishable_copy.header.stamp = ros::Time::now();
                mag_pub.publish(publishable_copy);
            }
        }
    }

    return 0;
}
