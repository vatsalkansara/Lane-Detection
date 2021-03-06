#!/usr/bin/env python
from __future__ import print_function
import roslib
#roslib.load_manifest('package.xml')
import sys
import rospy
import cv2
from std_msgs.msg import String
from sensor_msgs.msg import Image
from cv_bridge import CvBridge, CvBridgeError
import numpy as np

def nothing(x):
    pass
#cv2.namedWindow('areathr')
#cv2.createTrackbar('areath', 'areathr', 2000, 3000, nothing)

#kernel = np.ones((3,3),np.uint8)

class image_converter:

  def __init__(self):
    self.image_pub = rospy.Publisher("/camera/image_raw3",Image, queue_size=1)
    self.bridge = CvBridge()
    self.image_sub = rospy.Subscriber("/camera3/image_raw",Image,self.callback)#/camera3/usb_cam/image_raw

  def callback(self,data):
    
    try:
      cv_image = self.bridge.imgmsg_to_cv2(data, "bgr8")
    except CvBridgeError as e:
      print(e)
    
    #cv2.namedWindow('gaussorg')
    cv2.namedWindow('value-right')
    cv2.createTrackbar('area-th', 'value-right', 30, 1000, nothing)
    #cv2.createTrackbar('b', 'gaussorg', 60, 150, nothing)
    #cv2.createTrackbar('c', 'gaussorg', 69, 150, nothing) 
    cv2.createTrackbar('bv', 'value-right', 60, 150, nothing)
    cv2.createTrackbar('cv', 'value-right', 60, 150, nothing)
    cv2.createTrackbar('ksize', 'value-right', 1, 20, nothing)

    #fisheye correction part
    global mapx
    global mapy
    im1=cv2.remap(cv_image,mapx,mapy,cv2.INTER_LINEAR)
    #fisheye ends
    im1=cv2.resize(im1,(640,480))
    im1=im1[40:480,0:640]
    #cv2.rectangle(im1,(160,370),(280,440),0,-1)#CV_FILLED=-1
    #cv2.imshow('fisheyeright',im1)

    b, g, r = cv2.split(im1)
    #hsv_image = cv2.cvtColor(im1, cv2.COLOR_BGR2HSV)
    #h, s, v = cv2.split(hsv_image)
    bv = cv2.getTrackbarPos('bv', 'value-right')
    cv = cv2.getTrackbarPos('cv', 'value-right')
    #begin = rospy.get_time()
    v_th = cv2.adaptiveThreshold(b,255,cv2.ADAPTIVE_THRESH_GAUSSIAN_C,\
         cv2.THRESH_BINARY,(2*bv + 3),cv-100)
    #end = rospy.get_time()
    #print(end-begin)
    cv2.rectangle(v_th,(160,370),(280,440),0,-1)#CV_FILLED=-1
    ksize = cv2.getTrackbarPos('ksize', 'value-right')*2 + 1
    dst = cv2.medianBlur(v_th, ksize)
    cv2.imshow('value-right',dst)

    #im1 = cv2.blur(im1,(5,5))
    #cv2.imshow('areathr',im1)
    blank = np.zeros((im1.shape[0],im1.shape[1],3), np.uint8)
    rows,cols = blank.shape[:2]
    #img = cv2.cvtColor(im1, cv2.COLOR_BGR2GRAY)
    
    #thgauss = cv2.adaptiveThreshold(img,255,cv2.ADAPTIVE_THRESH_GAUSSIAN_C,\
         #cv2.THRESH_BINARY,(2*60 + 3),90-100)
    #cv2.rectangle(thgauss,(240,330),(400,480),0,-1)#CV_FILLED=-1
    
    #cv2.imshow('gaussorg',thgauss)
    thgauss2,contours,hierarchy = cv2.findContours(dst,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
    area = cv2.getTrackbarPos('area-th', 'value-right')

    for n, contours in enumerate(contours):
        if cv2.contourArea(contours) > area :
            #x,y,w,h = cv2.boundingRect(contours)
            #if True :
                #cv2.rectangle(im1,(x,y),(x+w,y+h),(0,255,0),2)
            cv2.drawContours(im1,[contours],-1,(0,255,0),2)
            cv2.drawContours(blank,[contours],-1,(255,255,255),-1)
            '''extLeft = tuple(contours[contours[:, :, 0].argmin()][0])
                extRight = tuple(contours[contours[:, :, 0].argmax()][0])
                [vx,vy,x,y] = cv2.fitLine(contours, cv2.DIST_L2,0,0.01,0.01)
                lefty = int(((extLeft[0]-x)*vy/vx) + y)
                righty = int(((extRight[0]-x)*vy/vx)+y)
                #cv2.line(blank,(extRight[0]-1,righty),(extLeft[0],lefty),(255,255,255),10)'''
                
                
    final = cv2.cvtColor(blank, cv2.COLOR_BGR2GRAY)
    cv2.imshow('fisheyeright',im1)
    cv2.imshow('Contour-right',final)
    cv2.waitKey(1)
    

    try:
      self.image_pub.publish(self.bridge.cv2_to_imgmsg(final, "mono8"))
    except CvBridgeError as e:
      print(e)

def main(args):
  ic = image_converter()
  rospy.init_node('image_processor', anonymous=True)
  try:
    rospy.spin()
  except KeyboardInterrupt:
    print("Shutting down")
    cv2.destroyAllWindows()

if __name__ == '__main__':
    npz_calib_file = np.load('/home/sine/catkin_ws/src/vatsal/src/rightcam_fisheye_correction.npz')
    distCoeff=npz_calib_file['distCoeff']
    intrinsic_matrix = npz_calib_file['intrinsic_matrix']
    npz_calib_file.close()
    newMat, ROI = cv2.getOptimalNewCameraMatrix(intrinsic_matrix,distCoeff,(1280,720),alpha=5.7,centerPrincipalPoint=1)
    mapx,mapy=cv2.initUndistortRectifyMap(intrinsic_matrix,distCoeff,None,newMat,(1280,720),m1type = cv2.CV_32FC1)
    print("file opened")
main(sys.argv)


