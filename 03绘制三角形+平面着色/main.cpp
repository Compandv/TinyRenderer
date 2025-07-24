#include <vector>
#include <cmath>
//#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0,   255, 0,   255);
Model *model = NULL;
const int width  = 800;
const int height = 800;

//画线算法(坐标1，坐标2，tga指针，颜色)
void line(Vec2i p0, Vec2i p1, TGAImage &image, TGAColor color) {
    bool steep = false;
    if (std::abs(p0.x-p1.x)<std::abs(p0.y-p1.y)) {
        //解决陡峭问题
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        steep = true;
    }
    if (p0.x>p1.x) {
        //解决反向画图
        std::swap(p0, p1);
    }

    for (int x=p0.x; x<=p1.x; x++) {
        float t = (x-p0.x)/(float)(p1.x-p0.x);
        int y = p0.y*(1.-t) + p1.y*t;
        if (steep) {
            image.set(y, x, color);
        } else {
            image.set(x, y, color);
        }
    }
}

//绘制三角形(坐标1，坐标2，坐标3，tga指针，颜色)
void triangle(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) {
    //三角形面积为0
    if (t0.y==t1.y && t0.y==t2.y) return;
    //根据y的大小对坐标进行排序
    if (t0.y>t1.y) std::swap(t0, t1);
    if (t0.y>t2.y) std::swap(t0, t2);
    if (t1.y>t2.y) std::swap(t1, t2);
    int total_height = t2.y-t0.y;
    //以高度差作为循环控制变量，此时不需要考虑斜率，因为着色完后每行都会被填充
    for (int i=0; i<total_height; i++) {
        //根据t1将三角形分割为上下两部分
        bool second_half = i>t1.y-t0.y || t1.y==t0.y;
        int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y;
        float alpha = (float)i/total_height;
        float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height; 
        //计算A,B两点的坐标
        Vec2i A =               t0 + (t2-t0)*alpha;
        Vec2i B = second_half ? t1 + (t2-t1)*beta : t0 + (t1-t0)*beta;
        if (A.x>B.x) std::swap(A, B);
        //根据A,B和当前高度对tga着色
        for (int j=A.x; j<=B.x; j++) {
            image.set(j, t0.y+i, color);
        }
    }
}

void triangle2(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage& image, TGAColor color) 
{
	//交换完毕之后，t0.v >= t1.v >= t2.v
    if (t1.v < t2.v) std::swap(t1, t2);
    if (t0.v < t1.v) std::swap(t0, t1);
	if (t1.v < t2.v) std::swap(t1, t2);

    //先画一半 采用水平从左往右画过去
    //中间点坐标为t1  通过y获取x坐标 两条线为t0->t2  和 t0->t1
    //先确定左边的线是哪条 如果左边的线太陡峭，就交换x,y
	Vec2i tt(t0), l(t2), r(t1);
	bool t1IsL = false, sleep = false;
	if (t2.u > t1.u)  l = t1, r = t2, t1IsL = true;
  //  //现在可以保证，t0与l的线在左边
  //  if (std::abs(tt.v - l.v) < std::abs(tt.u - l.u))
  //  {
		//std::swap(tt.u, tt.v);
		//std::swap(l.u, l.v);
  //      sleep = true;
  //  }
    for (int y = tt.v; y >= t1.v; --y)
	{
        float t = (y - tt.v) / (float)(t1.v - tt.v);
		int x1 = tt.u - (1. * (tt.v - l.v) / (tt.u - l.u)) * (y - tt.v);
		int x2 = tt.u - (1. * (tt.v - r.v) / (tt.u - r.u)) * (y - tt.v);
		line({ x1, y }, { x2, y }, image, color);
	}
    

}


int main(int argc, char** argv) {
    //读取model文件
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }
    TGAImage image(width, height, TGAImage::RGB);

	Vec3f light_dir(0, 0, -1); // 光线方向，此处为正对光源

	for (int i = 0; i < model->nfaces(); i++) {
		std::vector<int> face = model->face(i);
		Vec2i screen_coords[3]; // 屏幕坐标 用于后续着色
		Vec3f world_coords[3]; // 世界坐标 用于后续求法线，从而计算光线强度
		for (int j = 0; j < 3; j++) {
			Vec3f v = model->vert(face[j]);
			screen_coords[j] = Vec2i((v.x + 1.) * width / 2., (v.y + 1.) * height / 2.);
			world_coords[j] = v;
		}
		Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[2]); //
		n.normalize();
		float intensity = n * light_dir;
		if (intensity > 0) {
			triangle(screen_coords[0], screen_coords[1], screen_coords[2], image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255));
		}
	}

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

