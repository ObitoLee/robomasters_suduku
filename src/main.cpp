#include <opencv2/opencv.hpp>
#include <iostream>
#include "fstream"
#include "getConfig.h"
#include <sstream>

using namespace std;
using namespace cv;

#define DEBUG

#ifdef __unix__
#include "serialport.hpp"
Serialport stm32("/dev/ttyTHS0");
#endif

#define widthThreshold 45
#define heigtThreshold 26
#define sendTreshold 3 //连续判断5次结果相同才发送数据

int a = 170;//cany参数
int b = 11;

Mat_<float> intrinsic_matrix = (Mat_<float>(3, 3) << 530.31526, 0, 368.02664,
						        0, 531.36963, 274.08000,
						        0, 0, 1);
Mat_<float> distCoeffs = (Mat_<float>(5, 1) << -0.42437, 0.17281, -0.00660, 0.00088, 0.00000);

long matSum(Mat src)
{
    long sum = 0;
    for (int i = 0; i < src.rows; ++i)
    {
        uchar *p = src.ptr<uchar>(i);
        for (int j = 0; j < src.cols; ++j)
            sum += p[j];
    }
    return sum;
}

//九宫格中的一个小格
struct SudokuGrid
{
    Mat grid;
    Point2f gridCenter;
    long evaluation = 0;
    int lable;//聚类标签
    Point2f originSize;
};

void SudokuSort(vector<SudokuGrid> &sudoku)
{
    for (int i = 0; i < 9; ++i)//对y进行冒泡排序
        for (int j = 0; j < 8 - i; ++j)
            if (sudoku[j].gridCenter.y > sudoku[j + 1].gridCenter.y)
                swap(sudoku[j], sudoku[j + 1]);

    for (int k = 0; k < 3; ++k)//每3个一组，对x进行排序
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2 - i; ++j)
                if (sudoku[3 * k + j].gridCenter.x > sudoku[3 * k + j + 1].gridCenter.x)
                    swap(sudoku[3 * k + j], sudoku[3 * k + j + 1]);
}

int main(int argc,char* argv[])
{
    VideoCapture cap;
    if(argc == 1)
        cap.open(0);//打开摄像头
    else
        cap.open(argv[1]);

    if (!cap.isOpened())
    {
        cerr << "\n\e[31m\e[1m ERROR:Can't open camera or video! \e[0m\n\n";
        return -1;
    }

    Mat src, src0, thresh;

    ofstream out("out.txt");
#ifdef DEBUG
    namedWindow("src", CV_WINDOW_AUTOSIZE);
    cvCreateTrackbar("cany", "src", &a, 200);
    cvCreateTrackbar("threshold", "src", &b, 100);
#else
	map<string, string> m;
    ReadConfig("video.cfg", m);//读取配置文件
    string videoName;
    videoName += m["num"];
    videoName += ".avi";
    VideoWriter writer(videoName, CV_FOURCC('M', 'J', 'P', 'G'), 25, Size(640, 480));
    m["num"] = to_string( atoi(m["num"].c_str()) + 1);
    WriteConfig("video.cfg",m);//写入配置文件
#endif // DEBUG

    bool paused = false;
    vector<int> targetNum;
    targetNum.resize(sendTreshold);

    while (true)
    {
        if (!paused)
        {
            //cap >> src;
            cap.read(src0);
            if (src0.empty())
            {
                cerr << "read error!!\n";
                return -1;
            }
			
			if(argc == 1)
            	undistort(src0, src, intrinsic_matrix, distCoeffs);

#ifndef DEBUG
            writer<<src;
#endif // DEBUG
            //resize(src, src, Size(640, 480));//压缩
            Mat gray;
            cvtColor(src, gray, CV_BGR2GRAY);

            //adaptiveThreshold(gray, gray, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 177, 11);
            Canny(gray, thresh, a, 3 * a, 3);
            vector<vector<Point> > contours0;
            vector<RotatedRect> rectBorder;
            findContours(thresh, contours0, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
            drawContours(thresh, contours0, -1, Scalar(255, 255, 255));

            if (contours0.size() > 0)
            {
                vector<Point> poly;
                for (const auto& itContours : contours0)
                {
                    approxPolyDP(itContours, poly, 10, true);
                    //if (poly.size() == 4)
                    //{
                    rectBorder.push_back(minAreaRect(Mat(itContours)));
                    //}
                }

                vector<SudokuGrid> sudoku;//九宫格
                Mat rect9th[9];
				//@brief:识别九宫格
                for (const auto& rect : rectBorder)
                {
                    if (abs(rect.angle) < 5 || abs(rect.angle) > 85)
                    {
                        if (rect.center.x > 0.83 * src.cols || rect.center.x < 0.17 * src.cols ||
                                rect.center.y > 0.47 * src.rows || rect.center.y < 0.1 * src.rows)
                            continue;

                        int height = min(rect.size.height, rect.size.width);
                        int width = max(rect.size.height, rect.size.width);

                        if ((height > heigtThreshold) && (height < 2.5 * heigtThreshold) &&
                                (width > widthThreshold) && (width < 2 * widthThreshold) &&
                                (width < 2.2 * height) && (width > 1.5 * height))
                        {
                            //cout << "height:" << height << "   width:" << width << endl;
                            Rect objectBoundary = rect.boundingRect();
                            objectBoundary += Point(14, 7);
                            objectBoundary -= Size(29, 12);//修正
                            objectBoundary = objectBoundary & Rect(0, 0, src.cols, src.rows);//防止越界

                            if (objectBoundary.height < objectBoundary.width)
                            {
                                SudokuGrid temp;
                                temp.originSize = Point2f(objectBoundary.width, objectBoundary.height);
                                Mat roi(gray, objectBoundary);
                                resize(roi, roi, Size(32 * 2, 18 * 2));//压缩
                                temp.grid = roi;
                                temp.gridCenter = Point2f(objectBoundary.x, objectBoundary.y);

                                sudoku.push_back(temp);
                                rectangle(src, objectBoundary, Scalar(100, 200, 211), 3);
                            }
                            else
                            {
                                rectangle(src, objectBoundary, Scalar(100, 200, 100), 3);
                                cout<<height<<"    "<<width<<endl;
                            }
                        }
                        else
                        {
#ifdef DEBUG
                            Rect objectBoundary = rect.boundingRect();
                            objectBoundary += Point(17, 10);
                            objectBoundary -= Size(38, 19);//修正
                            objectBoundary = objectBoundary & Rect(0, 0, src.cols, src.rows);//防止越界
                            rectangle(src, objectBoundary, Scalar(200, 100, 211),3);
                            //cout << "不符合矩形的尺寸：" << rect.size << endl;
#endif // DEBUG
                        }
                    }
                }

                if (sudoku.size() > 9)//检测到大于9个，进行剔除
                {
//
//                    vector<Point2f> rectSize;//对矩形尺寸聚类
//
//                    for (const auto& item : sudoku)
//                        rectSize.push_back(item.originSize);
//
//                    const int K = 2;//聚类的类别数量，即Lable的取值
//                    Mat Label, Center;
//                    kmeans(rectSize, K, Label, TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 10, 1.0),
//                           1, KMEANS_PP_CENTERS, Center);//聚类3次，取结果最好的那次，聚类的初始化采用PP特定的随机算法。
//                           cout<<2222<<endl;
//                    int LableNum[K] = { 0 };//不同类别包含的成员数量，如类别0有8个，类别1有2个。。。。
//                    int sudoLable;//代表九宫格的标签
//                    for (int i = 0; i < rectSize.size(); ++i)
//                    {
//                        sudoku[i].lable = Label.at<int>(i);
//#ifdef DEBUG
//                        cout << rectSize[i] << "\t-->\t" << sudoku[i].lable << endl;
//#endif // DEBUG
//                        for (int j = 0; j < K; ++j)
//                            if (sudoku[i].lable == j)
//                            {
//                                LableNum[j]++;
//                                break;
//                            }
//                    }
//
//                    sudoLable = *max_element(LableNum, LableNum + K);
//
//                    for (int j = 0; j < K; ++j)
//                        if (LableNum[j] == sudoLable)
//                        {
//                            sudoLable = j;
//                            break;
//                        }
//
//                    vector<SudokuGrid> sudokuTemp;//九宫格
//                    for (int i = 0; i < rectSize.size(); ++i)
//                        if (sudoku[i].lable == sudoLable)
//                            sudokuTemp.push_back(sudoku[i]);
//
//                    sudoku.swap(sudokuTemp);
//                    sudokuTemp.~vector<SudokuGrid>();
//                    //cout << "九宫格属于类别：" << sudoLable << endl;
                }

                if (sudoku.size() == 9)//检测到9个
                {
					//对九宫格进行排序
                    SudokuSort(sudoku);
                    //求九宫格中evaluation最小的格子
                    int minResult = 1000000;
                    int minIndex = -1;
                    for (int i = 0; i < 9; ++i)
                    {
                        adaptiveThreshold(sudoku[i].grid, rect9th[i], 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 2*b+1, 3);
                        sudoku[i].evaluation = matSum(rect9th[i]);
#ifdef DEBUG
                        cout << i + 1 << ":  " << sudoku[i].evaluation << "  " << sudoku[i].gridCenter << endl;
                        char orderText[4];
                        sprintf(orderText, "%i", i+1);
                        putText(src, orderText, sudoku[i].gridCenter, FONT_HERSHEY_SCRIPT_SIMPLEX, 1, Scalar(0, 0, 255), 3, 8);
#endif // DEBUG
                        if (sudoku[i].evaluation < minResult)
                        {
                            minResult = sudoku[i].evaluation;
                            minIndex = i;
                        }
                    }

                    //求evaluation第二小的格子
                    int secondMinIndex = -1;
                    int secondMinResult = 1000000;
                    for (int i = 0; i < 9; ++i)
                    {
                        if (sudoku[i].evaluation < secondMinResult && i != minIndex)
                        {
                            secondMinResult = sudoku[i].evaluation;
                            secondMinIndex = i;
                        }
                    }

                    if (targetNum.back() == secondMinIndex+1 && (secondMinResult - minResult < 0.11 * minResult))
                    {
                        cout << "修正：用" << secondMinIndex + 1 << "替换" << minIndex + 1 << "  ";
                        minIndex = secondMinIndex;
                    }
#ifdef DEBUG
                    imshow("1", rect9th[minIndex]);
                    circle(src, sudoku[minIndex].gridCenter + Point2f(30, 16), 5, (0, 0, 255), 2, 8, 0);
#endif // DEBUG
                    targetNum.erase(targetNum.begin());
                    targetNum.push_back(minIndex + 1);
                    out << minIndex + 1;
                }
                else
                {
                    //targetNum.erase(targetNum.begin());
                    //targetNum.push_back(0);
                    cout << "符合条件矩形仅有：" << sudoku.size()  << endl;
                    out << " " ;
                }
                sudoku.clear();

                int i = 0;
                for (i = 1; i < targetNum.size(); i++)
                {
                    if (targetNum[i] != targetNum[0])
                        break;
                }
                if (i == sendTreshold)  //如果连续sendTreshold次检测结果一样，则向串口发送信号；
                {

#ifdef __unix__
                    char sendText[1];
                    sprintf(sendText, "%i", targetNum[0]);
                    stm32.send(sendText);
#endif // __unix__
                    cout << "target:  " << targetNum[0] << endl;
                    out << "  " << targetNum[0] << endl;

                }
                else
                {
                    out << "  " << "  " << endl;
                    cout << endl;
                }
            }
        }
#ifdef DEBUG
        imshow("src", src);
        imshow("thre", thresh);
#endif // DEBUG
        uchar key = uchar(waitKey(1));
        if (key == 'q')
            break;
        else if (key == ' ')
            paused = !paused;
    }
    out.close();
    return 0;
}
