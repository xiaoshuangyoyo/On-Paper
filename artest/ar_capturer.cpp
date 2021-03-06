//
// Created by 牛天睿 on 17/5/18.
//

#include "ar_capturer.h"




void on_paper::ar_capturer::main_loop() {
    TheVideoCapturer.open(0);
    int waitTime=1;
    if (!TheVideoCapturer.isOpened())  throw std::runtime_error("Could not open video");

    ///// CONFIGURE DATA
    // read first image to get the dimensions
    TheVideoCapturer >> TheInputImage;
    if (TheCameraParameters.isValid())
        TheCameraParameters.resize(TheInputImage.size());

    try {

        //  MDetector.setCornerRefinementMethod(aruco::MarkerDetector::SUBPIX);
        cv::namedWindow("ar");
        //go!
        char key = 0;
        int index = 0;
        // capture until press ESC or until the end of the video

        TheLastMarkers=MDetector.detect(TheInputImage, TheCameraParameters, TheMarkerSize);


        do {

            TheVideoCapturer.retrieve(TheInputImage);
            // copy image
            // Detection of markers in the image passed
            TheMarkers= MDetector.detect(TheInputImage, TheCameraParameters, TheMarkerSize);

            TheInputImage.copyTo(TheInputImageCopy);
            fill_markers();
            if(perform_anti_shake)
                anti_shake();
            map_markers();
            TheLastMarkers=TheMarkers;
            cv::imshow("ar", resize(TheInputImageCopy,1280));

            key = (char)cv::waitKey(1); // wait for key to be pressed
            if(key=='s')  {
                waitTime= waitTime==0?1:0;
            } else if(key == 'a'){
                this->perform_anti_shake = (not this->perform_anti_shake);
            }
            index++; // number of images captured

            //更新上一帧图像的Marker
            //TheInputImage.copyTo(ThePreImage);


        } while (key != 27 && (TheVideoCapturer.grab() ));

    } catch (std::exception &ex)

    {
        cout << "Exception :" << ex.what() << endl;
    }



}

void on_paper::ar_capturer::init() {

    image = imread("../x-0.png");
    //image= readPDFtoCV("../rt.pdf", 300);
    cvtColor(image, image, CV_BGR2BGRA);
    // read camera parameters if passed
    TheCameraParameters.readFromXMLFile("../camera.yml");

    auto&& get_shifted_paper_pattern=[](float ratio_x, float ratio_y, const vector<Point2f> vp2f){
        assert(vp2f.size()==4);
        float height = vp2f[0].y*2;
        float width  = vp2f[0].x*2;
        float shifty = (height*ratio_y),
                shiftx= (width*ratio_x);

        vector<Point2f> ps ={
                Point2f(vp2f[0].x+shiftx, vp2f[0].y+shifty),
                Point2f(vp2f[1].x+shiftx, vp2f[1].y+shifty),
                Point2f(vp2f[2].x+shiftx, vp2f[2].y+shifty),
                Point2f(vp2f[3].x+shiftx, vp2f[3].y+shifty)
        };
        return std::move(ps);

    };
    //add shifted patterns.
    for(auto& s : shifts)
        this->shifted_pattern_paper_source.emplace_back(
                get_shifted_paper_pattern(s.first, s.second, pattern_paper_source));

    this->original_image_pattern = {
            Point2f(image.cols, image.rows),
            Point2f(0, image.rows),
            Point2f(0, 0),
            Point2f(image.cols, 0)
    };
    MDetector.setDictionary("ARUCO");
    MDetector.setThresholdParams(7, 7);
    MDetector.setThresholdParamRange(2, 0);
    this->perform_anti_shake = false;

}

void on_paper::ar_capturer::
overlayImage(const cv::Mat &background, const cv::Mat &foreground,
             cv::Mat &output, cv::Point2i location) {
    background.copyTo(output);
    // start at the row indicated by location, or at row 0 if location.y is negative.
    for(int y = std::max(location.y , 0); y < background.rows; ++y)
    {
        int fY = y - location.y; // because of the translation

        // we are done of we have processed all rows of the foreground image.
        if(fY >= foreground.rows)
            break;
        // start at the column indicated by location,
        // or at column 0 if location.x is negative.
        for(int x = std::max(location.x, 0); x < background.cols; ++x)
        {
            int fX = x - location.x; // because of the translation.

            // we are done with this row if the column is outside of the foreground image.
            if(fX >= foreground.cols)
                break;

            // determine the opacity of the foregrond pixel, using its fourth (alpha) channel.
            double opacity =
                    ((double)foreground.data[fY * foreground.step + fX * foreground.channels() + 3])

                    / 255.;
            // and now combine the background and foreground pixel, using the opacity,
            // but only if opacity > 0.
            for(int c = 0; opacity > 0 && c < output.channels(); ++c)
            {
                unsigned char foregroundPx =
                        foreground.data[fY * foreground.step + fX * foreground.channels() + c];
                unsigned char backgroundPx =
                        background.data[y * background.step + x * background.channels() + c];
                output.data[y*output.step + output.channels()*x + c] =
                        backgroundPx * (1.-opacity) + foregroundPx * opacity;
            }
        }
    }

}
#define S2(X) ((X)*(X))
template<typename vecpf>
float on_paper::ar_capturer::euclid_dist(const vecpf &v1, const vecpf &v2) {
    if(v1.size()!=v2.size())return -1;
    float acc=0;
    for(auto i = 0; i< v1.size();i++){
        auto& p1=v1[i];
        auto& p2=v2[i];
        acc+= sqrt(S2(p1.x-p2.x)+S2(p1.y-p2.y));
    }
    return acc/v1.size();
}
#undef S2


void on_paper::ar_capturer::white_transparent(const cv::Mat &src, cv::Mat &dst) {
    cv::cvtColor(src, dst, CV_BGR2BGRA);
    // find all white pixel and set alpha value to zero:
    for (int y = 0; y < dst.rows; ++y)
        for (int x = 0; x < dst.cols; ++x)
        {
            cv::Vec4b & pixel = dst.at<cv::Vec4b>(y, x);
            // if pixel is white
            if (pixel[0] > 250 && pixel[1] > 250 && pixel[2] > 250)
            {
                // set alpha to zero:
                pixel[3] = 0;
            }
        }
}

cv::Mat on_paper::ar_capturer::resize(const cv::Mat &in, int width)
{
        if (in.size().width<=width) return in;
        float yf=float(  width)/float(in.size().width);
        cv::Mat im2;
        cv::resize(in,im2,cv::Size(width,float(in.size().height)*yf));
        return im2;
}



/**
 * @brief 把所得的marker映射到纸上。
 * 将会把marker映射到的四个点进行平均。
 * 遵循以下策略：
 * 1. 如果没有中间部分的marker，使用两个角上的任意一个marker。
 * 2. 如果存在中间部分的marker，使用两个marker映射结果的平均值。(TODO 加上两个角上的marker的干预)
 */
void on_paper::ar_capturer::map_markers(void) {

    bool exists_in_middle = false;
    if (TheMarkers.size() > 0 && TheCameraParameters.isValid() && TheMarkerSize > 0)
    {
        for(auto& m : TheMarkers){
            if(m.id%4==1 or m.id%4 ==2)//中间部分的marker
            exists_in_middle = true;
        }

    } else return ;//do nothing and back!

    vector<Point2f> virtual_paper ;
    for (unsigned int i = 0; i < TheMarkers.size(); i++) {

        Marker &lamaker = TheMarkers[i];
        //如果存在中间点，则忽略两侧的点。
        if(exists_in_middle and (lamaker.id%4 == 0 or lamaker.id%4 == 3))
            continue;

        Point mcenter = lamaker.getCenter();
        if (lamaker.size() != 4) {
            std::cout << "Panic! " << std::endl;
            exit(-9);
        }
        Mat M0=getPerspectiveTransform(pattern_marker_source, lamaker);

        vector<Point2f> __virtual_paper;
        perspectiveTransform(shifted_pattern_paper_source[lamaker.id%4], __virtual_paper, M0);

        //circle(TheInputImageCopy, mcenter, 10, Scalar(0,255,0),10);
        //for(auto ii=0;ii< lamaker.size();i++)
        //            circle(TheInputImageCopy, lamaker[i], 10, Scalar(0, i*50, 0), 10);

        if(virtual_paper.size()==0)
            virtual_paper=__virtual_paper;
        else
            //如果我们有两个marker，用它进行平均。
            virtual_paper = vector_avg2(virtual_paper, __virtual_paper);
    }

    //把原图投射到虚拟纸张上。
    Mat M = getPerspectiveTransform(original_image_pattern, virtual_paper);
    Mat transf = Mat::zeros(TheInputImageCopy.size(), CV_8UC4);
    warpPerspective(image, transf, M, TheInputImageCopy.size(), cv::INTER_NEAREST);
    white_transparent(transf, transf);
    //Mat output;
    overlayImage(TheInputImageCopy, transf, TheInputImageCopy, Point(0, 0));
}

vector<cv::Point2f> on_paper::ar_capturer::vector_avg2(const vector<cv::Point2f> &src1, const vector<cv::Point2f> &src2)
{
    vector<Point2f> dst;
    if(src1.size()!= src2.size()){
        cout<<"Vectors must be of same size!"<<endl;
        exit(999);
    }
    for(auto i = 0;i<src1.size();i++)
        dst.push_back(Point2f((src1[i].x+src2[i].x)/2, (src1[i].y+src2[i].y)/2));
    return dst;
}

void on_paper::ar_capturer::anti_shake(void) {
#define distance(a,b) (sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)))
    for (unsigned int i = 0; i < TheMarkers.size()&&i<TheLastMarkers.size(); i++) {
        Point center=TheMarkers[i].getCenter();
        Point pre_center=TheLastMarkers[i].getCenter();
        if(distance(center,pre_center)<10)
            TheMarkers[i]=TheLastMarkers[i];
    }
}

void on_paper::ar_capturer::fill_markers(void) {
    for(auto& lamaker : TheMarkers) {//fill the marker with white.
        constexpr const int covering_shift = 10;
        vector<Point> corners(lamaker.begin(), lamaker.end());
        corners[0].x-=covering_shift;corners[0].y-=covering_shift;
        corners[1].x+=covering_shift;corners[1].y-=covering_shift;
        corners[2].x+=covering_shift;corners[2].y+=covering_shift;
        corners[3].x-=covering_shift;corners[3].y+=covering_shift;

        vector<vector<Point>> c_corners = {corners};
        fillPoly(TheInputImageCopy, c_corners, Scalar(180, 180, 180));
    }
}

#undef distance

