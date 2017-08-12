#include "image_properties.h"
#include "nav_msgs/OccupancyGrid.h"
#include "math.h" 
//Store all constants for image encodings in the enc namespace to be used later.
namespace enc = sensor_msgs::image_encodings;

using namespace cv;
using namespace std;
 
//Declare a string with the name of the window that we will create using OpenCV where processed images will be displayed.
static const char WINDOW[] = "Image Processed";

//Use method of ImageTransport to create image publisher
image_transport::Publisher pub;

nav_msgs::OccupancyGrid Final_Grid;

ros::Subscriber sub_Lanedata;
ros::Publisher pub_Lanedata;

int sep = 50;
int interpol_height = 30;
int epsilon = 6;
int angle = 20;
double begin,end;
Mat src, src_gray;
Mat src_rgb;

void extend(std::vector<std::vector<Point> > &Lane_points)
{
    for (int i = 0; i < Lane_points.size(); ++i)
    {
        int max_j = 0;

        for (int j = 0; j < Lane_points[i].size(); ++j)
        {
            if (Lane_points[i][max_j].y < Lane_points[i][j].y)
            {
                max_j = j;
            }
        }

        int k = 0;//-1

        for (int j = 0; j < Lane_points[i].size(); ++j)
        {
            int diffx = Lane_points[i][max_j].x - Lane_points[i][j].x;
            int diffy = Lane_points[i][max_j].y - Lane_points[i][j].y;

            float dist = sqrt(diffx*diffx + diffy*diffy);

            if (dist > sep)
            {
                k = j;
            }

            if ((j == Lane_points[i].size() - 1) && (dist < sep) && (k == -1))
            {
                k = -1;
            }
        }

        if ((k != -1) && (Lane_points[i][max_j].y > occ_grid_heightr - interpol_height))
        {
            Point pt;
            float m, c;

            if ((float)Lane_points[i][k].y != (float)Lane_points[i][max_j].y)
            {
                m = ((float)Lane_points[i][k].x - (float)Lane_points[i][max_j].x)/((float)Lane_points[i][k].y - (float)Lane_points[i][max_j].y);
                c = (float)Lane_points[i][max_j].x - m*(float)Lane_points[i][max_j].y;

                pt.y = occ_grid_heightr - 1;
                pt.x = (int)(m*(float)pt.y + c);

                if ((Lane_points[i][max_j].x < image_width/2) && (Lane_points[i][k].x < image_width/2) && (pt.x < image_width/2) && (abs(m) < tan(angle*CV_PI/180.0)))
                line(src, Lane_points[i][max_j], pt, Scalar(255, 255, 255));

                else if ((Lane_points[i][max_j].x > image_width/2) && (Lane_points[i][k].x > image_width/2) && (pt.x > image_width/2) && (abs(m) < tan(angle*CV_PI/180.0)))
                line(src, Lane_points[i][max_j], pt, Scalar(255, 255, 255));
                    
                else
                {
                    pt.y = occ_grid_heightr - 1;
                    pt.x = Lane_points[i][max_j].x;

                    line(src, Lane_points[i][max_j], pt, Scalar(255, 255, 255));
                }

                circle(src_rgb, Lane_points[i][k], 6, Scalar(255, 255, 0));
                circle(src_rgb, Lane_points[i][max_j], 6, Scalar(255, 255, 0));
                circle(src_rgb, pt, 6, Scalar(255, 255, 0));
                line(src_rgb, Lane_points[i][k+1], Lane_points[i][1], Scalar(255, 255, 0));

                // cout<<m<<endl;
            }

            else
            {
                pt.y = occ_grid_heightr - 1;
                pt.x = Lane_points[i][max_j].x;

                line(src, Lane_points[i][max_j], pt, Scalar(255, 255, 255));
            }
        }

        else if ( Lane_points[i][max_j].y > occ_grid_heightr - interpol_height)
        {
            Point pt;

            pt.y = occ_grid_heightr - 1;
            pt.x = Lane_points[i][max_j].x;

            line(src, Lane_points[i][max_j], pt, Scalar(255, 255, 255));
        }
    }
}


void interpolate(int, void*)
{
    std::vector<std::vector<Point> > contours;
    std::vector<std::vector<Point> > Lane_points;
    std::vector<Vec4i> hierarchy;

    cvtColor(src, src_gray, CV_BGR2GRAY);
    findContours(src_gray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_NONE);

    src_rgb = Mat::zeros(Size(occ_grid_widthr, occ_grid_heightr), CV_8UC3);

    for (int i = 0; i < contours.size(); ++i)
    {
        std::vector<Point> vec;

        approxPolyDP(Mat(contours[i]), vec, (double)epsilon, true);

        Lane_points.push_back(vec);
    }

    extend(Lane_points);

    for (int i = 0; i < Lane_points.size(); ++i)
    {
        circle(src_rgb, Lane_points[i][0], 4, Scalar(255, 255, 0));
        circle(src_rgb, Lane_points[i][Lane_points[i].size() - 1], 4, Scalar(255, 255, 255));

        for (int j = 0; j < Lane_points[i].size() - 1; ++j)
        {
            line(src_rgb, Lane_points[i][j], Lane_points[i][j+1], Scalar(255, 255, 255), 1);
        }
    }
    Final_Grid.info.map_load_time = ros::Time::now();
    Final_Grid.header.frame_id = "lane";
    Final_Grid.info.resolution = (float)map_width/(100*(float)occ_grid_widthr);
    Final_Grid.info.width = 200;
    Final_Grid.info.height = 400;

    Final_Grid.info.origin.position.x = 0;
    Final_Grid.info.origin.position.y = 0;
    Final_Grid.info.origin.position.z = 0;

    Final_Grid.info.origin.orientation.x = 0;
    Final_Grid.info.origin.orientation.y = 0;
    Final_Grid.info.origin.orientation.z = 0;
    Final_Grid.info.origin.orientation.w = 1;

    cvtColor(src,src_gray, CV_BGR2GRAY);

    for (int i = 0; i < src_gray.rows; ++i)
    {
        for (int j = 0; j < src_gray.cols; ++j)
        {
            if ( src_gray.at<uchar>(i,j) > 0)
            {
                //cout<<"r";
                Final_Grid.data.push_back(2);

            }
            else
                Final_Grid.data.push_back(src_gray.at<uchar>(i,j));
        }
    }   

    pub_Lanedata.publish(Final_Grid);
    waitKey(1);
    imshow(WINDOW, src);
    imshow("src_rgb", src_rgb);

    Final_Grid.data.clear();

    Lane_points.clear();
}

//This function is called everytime a new image is published
void imageCallback(const sensor_msgs::ImageConstPtr& original_image)
{
    //Convert from the ROS image message to a CvImage suitable for working with OpenCV for processing
    //begin = ros::Time::now().toSec();
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
        //Always copy, returning a mutable CvImage
        //OpenCV expects color images to use BGR channel order.
        cv_ptr = cv_bridge::toCvCopy(original_image, enc::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
        //if there is an error during conversion, display it
        ROS_ERROR("videofeed::main.cpp::cv_bridge exception: %s", e.what());
        return;
    };
 
    //Display the image using OpenCV
    cv::imshow(WINDOW, cv_ptr->image);

    src = cv_ptr->image;

    interpolate(0,0);
    //end=ros::Time::now().toSec();
    //cout<< end - begin <<endl;

    //Add some delay in miliseconds. The function only works if there is at least one HighGUI window created and the window is active. If there are several HighGUI windows, any of them can be active.
    cv::waitKey(3);

    /**
    * The publish() function is how you send messages. The parameter
    * is the message object. The type of this object must agree with the type
    * given as a template parameter to the advertise<>() call, as was done
    * in the constructor in main().
    */
    //Convert the CvImage to a ROS image message and publish it on the "camera/image_processed" topic.
}
 
/**
* This tutorial demonstrates simple image conversion between ROS image message and OpenCV formats and image processing
*/
int main(int argc, char **argv)
{

    /**
    * The ros::init() function needs to see argc and argv so that it can perform
    * any ROS arguments and name remapping that were provided at the command line. For programmatic
    * remappings you can use a different version of init() which takes remappings
    * directly, but for most command-line programs, passing argc and argv is the easiest
    * way to do it.  The third argument to init() is the name of the node. Node names must be unique in a running system.
    * The name used here must be a base name, ie. it cannot have a / in it.
    * You must call one of the versions of ros::init() before using any other
    * part of the ROS system.
    */
        ros::init(argc, argv, "Interpolater");
    /**
    * NodeHandle is the main access point to communications with the ROS system.
    * The first NodeHandle constructed will fully initialize this node, and the last
    * NodeHandle destructed will close down the node.
    */
        ros::NodeHandle nh;

    //Create an ImageTransport instance, initializing it with our NodeHandle.
        image_transport::ImageTransport it(nh);
    //OpenCV HighGUI call to create a display window on start-up.
    cv::namedWindow(WINDOW, CV_WINDOW_AUTOSIZE);
    /**
    * Subscribe to the "camera/image_raw" base topic. The actual ROS topic subscribed to depends on which transport is used.
    * In the default case, "raw" transport, the topic is in fact "camera/image_raw" with type sensor_msgs/Image. ROS will call
    * the "imageCallback" function whenever a new image arrives. The 2nd argument is the queue size.
    * subscribe() returns an image_transport::Subscriber object, that you must hold on to until you want to unsubscribe.
    * When the Subscriber object is destructed, it will automatically unsubscribe from the "camera/image_raw" base topic.
    */
    image_transport::Subscriber sub = it.subscribe("/Interpolater", 1, imageCallback);
    //image_transport::Subscriber sub = it.subscribe("camera/image_raw", 1, imageCallback);
    //OpenCV HighGUI call to destroy a display window on shut-down.
    cv::destroyWindow(WINDOW);

    pub_Lanedata = nh.advertise<nav_msgs::OccupancyGrid>("/Lane_Occupancy_Grid", 1);
    /**
    * The advertise() function is how you tell ROS that you want to
    * publish on a given topic name. This invokes a call to the ROS
    * master node, which keeps a registry of who is publishing and who
    * is subscribing. After this advertise() call is made, the master
    * node will notify anyone who is trying to subscribe to this topic name,
    * and they will in turn negotiate a peer-to-peer connection with this
    * node.  advertise() returns a Publisher object which allows you to
    * publish messages on that topic through a call to publish().  Once
    * all copies of the returned Publisher object are destroyed, the topic
    * will be automatically unadvertised.
    *
    * The second parameter to advertise() is the size of the message queue
    * used for publishing messages.  If messages are published more quickly
    * than we can send them, the number here specifies how many messages to
    * buffer up before throwing some away.
    */

    /**
    * In this application all user callbacks will be called from within the ros::spin() call.
    * ros::spin() will not return until the node has been shutdown, either through a call
    * to ros::shutdown() or a Ctrl-C.
    */
        ros::spin();
    //ROS_INFO is the replacement for printf/cout.
    ROS_INFO("videofeed::main.cpp::No error.");
 
}