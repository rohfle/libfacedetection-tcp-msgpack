/*
By downloading, copying, installing or using the software you agree to this license.
If you do not agree to this license, do not download, install,
copy or use the software.


                  License Agreement For libfacedetection
                     (3-clause BSD License)

Copyright (c) 2018-2020, Shiqi Yu, all rights reserved.
shiqi.yu@gmail.com

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are disclaimed.
In no event shall copyright holders or contributors be liable for any direct,
indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.
*/

#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <msgpack.hpp>

#include "facedetectcnn.h"

//define the buffer size. Do not change the size!
#define DETECT_BUFFER_SIZE 0x20000
using namespace cv;


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <ctime>


#define DEBUG_IMAGE
#define SERVERPORT 7071

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s <camera index>\n", argv[0]);
        return -1;
    }

	int * pResults = NULL;
    //pBuffer is used in the detection functions.
    //If you call functions in multiple threads, please create one buffer for each thread!
    unsigned char * pBuffer = (unsigned char *)malloc(DETECT_BUFFER_SIZE);
    if(!pBuffer)
    {
        fprintf(stderr, "Can not alloc buffer.\n");
        return -1;
    }

    /// CODE CHUNK
    // setup network socket
    struct sockaddr_in send_addr, recv_addr;
    int fd;

    memset(&send_addr, 0, sizeof send_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = (in_port_t) htons(SERVERPORT);
    // broadcasting address for unix (?)
    send_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bool connected = false;
    /// END CODE CUNK

    VideoCapture cap;
    Mat im;

    if( isdigit(argv[1][0]))
    {
        cap.open(argv[1][0]-'0');
        if(! cap.isOpened())
        {
            cerr << "Cannot open the camera." << endl;
            return 0;
        }
    }

    if( cap.isOpened())
    {
        while(true)
        {
            cap >> im;
            //cout << "Image size: " << im.rows << "X" << im.cols << endl;
            Mat image = im.clone();

            ///////////////////////////////////////////
            // CNN face detection
            // Best detection rate
            //////////////////////////////////////////
            //!!! The input image must be a BGR one (three-channel) instead of RGB
            //!!! DO NOT RELEASE pResults !!!
            TickMeter cvtm;
            cvtm.start();

            pResults = facedetect_cnn(pBuffer, (unsigned char*)(image.ptr(0)), image.cols, image.rows, (int)image.step);

            cvtm.stop();
            printf("time = %gms\n", cvtm.getTimeMilli());

            printf("%d faces detected.\n", (pResults ? *pResults : 0));
            Mat result_image = image.clone();
            //print the detection results

            /// CODE CHUNK
            // create network connection, handle reconnects
            if (!connected) {
                if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    perror("socket creation failed...trying in 5 seconds\n");
                    sleep(5);
                    continue;
                }

                if (connect(fd, (const sockaddr*)&send_addr, sizeof(send_addr)) != 0) {
                    perror("connection with the server failed...trying in 5 seconds\n");
                    close(fd);
                    sleep(5);
                    continue;
                }

                printf("Connected!\n");
                connected = true;
            }

            // for encode jpeg
            std::vector<uchar> buff;
            // pack metadata and image
            std::stringstream ss;
            std::vector<std::map<std::string, int>> meta;
            /// END CODE CHUNK

            for(int i = 0; i < (pResults ? *pResults : 0); i++)
            {
                std::map<std::string, int> matchMeta;

                short * p = ((short*)(pResults+1))+142*i;
                int confidence = p[0];
                int x = p[1];
                int y = p[2];
                int w = p[3];
                int h = p[4];
                /// CODE CHUNK
                // pack detection meta
                matchMeta["cnf"] = p[0];
                matchMeta["x"] = p[1];
                matchMeta["y"] = p[2];
                matchMeta["w"] = p[3];
                matchMeta["h"] = p[4];

                meta.push_back(matchMeta);
                /// END CODE CHUNK

                //show the score of the face. Its range is [0-100]
#ifdef DEBUG_IMAGE
                char sScore[256];
                snprintf(sScore, 256, "%d", confidence);
                cv::putText(result_image, sScore, cv::Point(x, y-3), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

                //draw face rectangle
                rectangle(result_image, Rect(x, y, w, h), Scalar(0, 255, 0), 2);
                //draw five face landmarks in different colors
                cv::circle(result_image, cv::Point(p[5], p[5 + 1]), 1, cv::Scalar(255, 0, 0), 2);
                cv::circle(result_image, cv::Point(p[5 + 2], p[5 + 3]), 1, cv::Scalar(0, 0, 255), 2);
                cv::circle(result_image, cv::Point(p[5 + 4], p[5 + 5]), 1, cv::Scalar(0, 255, 0), 2);
                cv::circle(result_image, cv::Point(p[5 + 6], p[5 + 7]), 1, cv::Scalar(255, 0, 255), 2);
                cv::circle(result_image, cv::Point(p[5 + 8], p[5 + 9]), 1, cv::Scalar(0, 255, 255), 2);
#endif
                //print the result
                printf("face %d: confidence=%d, [%d, %d, %d, %d] (%d,%d) (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n",
                        i, confidence, x, y, w, h,
                        p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13],p[14]);

            }

            /// CODE CHUNK
            // create jpeg
            imencode(".jpg", result_image, buff);
            // pack jpeg and detection meta using msgpack
            std::tuple<
                std::vector<uchar>,
                std::vector<std::map<std::string, int>>> togo {buff, meta};
            msgpack::pack(ss, togo);
            std::string data = ss.str();
            // send packed data via socket
            cout << "Sending stuff with length: " << data.length() << endl;
            if (write(fd, data.c_str(), data.length()) < 0) {
                perror("error while writing data");
                close(fd);
                connected = false;
            }
            /// END CODE CHUNK
            // imshow("result", result_image);
        }
    }

    //release the buffer
    free(pBuffer);

	return 0;
}
