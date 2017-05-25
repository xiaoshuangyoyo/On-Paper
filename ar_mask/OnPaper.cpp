//
// Created by 牛天睿 on 17/5/22.
//

#include "OnPaper.h"

void on_paper::OnPaper::main_loop(void) {

    TheVideoCapturer.open(0);

    TheVideoCapturer.set(CV_CAP_PROP_FRAME_WIDTH, 800);
    TheVideoCapturer.set(CV_CAP_PROP_FRAME_HEIGHT, 600);

    int waitTime=1;
    if (!TheVideoCapturer.isOpened())  throw std::runtime_error("Could not open video");

    ///// CONFIGURE DATA
    // read first image to get the dimensions
    TheVideoCapturer >> TheInputImage;
    if (TheCameraParameters.isValid())
        TheCameraParameters.resize(TheInputImage.size());

    //this->pa.init(TheInputImage.rows, TheInputImage.cols);

    tb.init(TheInputImage.rows, TheInputImage.cols);


    Scalar line_color = Scalar(255, 255, 0);
    tb.register_callback("gored", Point(200,0), Point(400,150), [this, &line_color]{
        line_color = Scalar(0,0,255);
    });


    try {


        cv::namedWindow("ar");
        //cv::namedWindow("mask");
        //go!
        char key = 0;
        int index = 0;
        // capture until press ESC or until the end of the video

        Mat mask ; //HandDetector的mask参数，据说目前还没啥用。
        do {

            TheVideoCapturer.retrieve(TheInputImage);
            //do something.
            ac.get_input_image(TheInputImage);
            auto mknum = ac.process();//num of markers.
            GestureType gt = gj.get_gesture(TheInputImage);
            mask = gj.mask;
            Point finger_tip=Point(0,0);

            if(gt != GestureType::NONE)
                finger_tip = gj.key_point();


            // let pa to rock.
            if(mknum > 0) //detected markers!
            {
                tb.fire_event(finger_tip);
                pa.with_transmatrix(ac.get_transmatrix_inv());
                if(gt == GestureType::PRESS)
                    pa.kalman_trace(finger_tip, 5, line_color, true);
                elif(gt == GestureType::MOVE)
                    pa.kalman_trace(finger_tip, 5, line_color, false);

                pa.transform_canvas(ac.get_transmatrix(), TheInputImage.size());
            }

            //Overlay!

            lm.capture(ac.get_processed_image());
            if(mknum>0) {
                lm.capture(ac.get_virtual_paper_layer());
                lm.capture(pa.get_canvas_layer());
            }

            lm.overlay();
            lm.output(TheProcessedImage);
            if(gt!=GestureType::NONE)
                circle(TheProcessedImage, finger_tip, 4, Scalar(0, 0, 255), 4);


            cv::imshow("ar", TheProcessedImage);
            //cv::imshow("mask", mask);

            key = (char)cv::waitKey(1); // wait for key to be pressed
            if(key=='s')  {
                waitTime= waitTime==0?1:0;
            } else if(key == 'a')
                ac.toggle_anti_shake();

            index++; // number of images captured

        } while (key != 27 && (TheVideoCapturer.grab() ));

    } catch (std::exception &ex)

    {
        cout << "Exception :" << ex.what() << endl;
    }


}

