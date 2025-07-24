#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model        = NULL;
float *shadowbuffer = NULL;

const int width  = 800;
const int height = 800;

Vec3f light_dir(-1,2,3);
Vec3f       eye(1,2,8);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

struct Shader : public IShader {
    mat<4,4,float> uniform_M;   //  Projection*ModelView
    mat<4,4,float> uniform_MIT; // (Projection*ModelView).invert_transpose() 视角变换矩阵的逆转置矩阵，可以得到变换之后的法线向量
    mat<4,4,float> uniform_Mshadow; // 这个片面的屏幕坐标转为阴影buffer下的屏幕坐标的变换矩阵 
    mat<2,3,float> varying_uv;  // 三角形各顶点uv
    mat<3,3,float> varying_tri; // 三角形各顶点屏幕坐标

    Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport*Projection*ModelView*embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        //uniform_Mshadow = M1 * M2
        //M1为输入坐标转为阴影buffer下的屏幕坐标的变换矩阵  M2为输入坐标转为一般屏幕坐标的变换矩阵的逆
        // M1 * M2 * 屏幕坐标 = M1 * (M2 * 屏幕坐标) = M1 * 空间坐标 = 阴影buffer下的屏幕坐标的变换矩阵
        Vec4f sb_p = uniform_Mshadow*embed<4>(varying_tri*bar); 
        sb_p = sb_p/sb_p[3];
        // 阴影buffer数组的索引
        int idx = int(sb_p[0]) + int(sb_p[1])*width; 
        //对比这个像素z坐标与阴影buffer  --》 pz  yyb
        // //若 yyb < pz 说明这个点没有被遮住（shadow系数就为1，即不用打阴影，颜色没有因阴影而改变）
        // //若 yyb > pz 说明这个点被遮挡住了（shadow系数就为0.3，打阴影，颜色会稍微深一些，见下面计算公式）
        float shadow = .5+.5*(shadowbuffer[idx]<sb_p[2]); 
        Vec2f uv = varying_uv*bar;                 // 当前像素的uv插值
        Vec3f n = proj<3>(uniform_MIT*embed<4>(model->normal(uv))).normalize(); // normal
        Vec3f l = proj<3>(uniform_M  *embed<4>(light_dir        )).normalize(); // light vector
        Vec3f r = (n*(n*l*2.f) - l).normalize();   // reflected light
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv)); //为什么没r * v，而是r.z
        float diff = std::max(0.f, n*l);
        TGAColor c = model->diffuse(uv);
		TGAColor glow = model->glow(uv);
        //
        for (int i=0; i<3; i++) color[i] = std::min<float>(20 + glow[i] * 10 + c[i]*shadow*(1.2*diff + .6 * spec), 255);//加入阴影
        return false;
    }
};

struct DepthShader : public IShader {
    mat<3,3,float> varying_tri;

    DepthShader() : varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;          // transform it to screen coordinates
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec3f p = varying_tri*bar;
        color = TGAColor(255, 255, 255)*(p.z/depth);
        return false;
    }
};

int main(int argc, char** argv) {
    //if (2>argc) {
    //    std::cerr << "Usage: " << argv[0] << "obj/african_head.obj" << std::endl;
    //    return 1;
    //}

    float *zbuffer = new float[width*height];
    shadowbuffer   = new float[width*height];
    for (int i=width*height; --i; ) {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }

    //model = new Model(argv[1]);

	//加载模型
	if (2 == argc) {
		model = new Model(argv[1]);
	}
	else {
		model = new Model("obj/diablo3_pose.obj");
	}

    light_dir.normalize();

    { 
        //阴影着色，和一般的着色其实类似
        //相当于从从光源处看过去，Zbuffer靠近的位置颜色会亮，看不到的位置会有阴影
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(light_dir, center, up);
        viewport(width/8, height/8, width*3/4, height*3/4);
        projection(0);

        DepthShader depthshader;
        Vec4f screen_coords[3];
        for (int i=0; i<model->nfaces(); i++) {
            for (int j=0; j<3; j++) {
                screen_coords[j] = depthshader.vertex(i, j);
            }
            triangle(screen_coords, depthshader, depth, shadowbuffer);
        }
        depth.flip_vertically();
        depth.write_tga_file("depth.tga");
    }

    Matrix M = Viewport*Projection*ModelView;//输入坐标转为阴影缓冲区中屏幕坐标

    { // rendering the frame buffer
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(eye, center, up);
        viewport(width/8, height/8, width*3/4, height*3/4);
        projection(-1.f/(eye-center).norm());

        /*
        * Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}
        *
        */
        Shader shader(ModelView, (Projection*ModelView).invert_transpose(), M*(Viewport*Projection*ModelView).invert());
        Vec4f screen_coords[3];
        for (int i=0; i<model->nfaces(); i++) {
            for (int j=0; j<3; j++) {
                screen_coords[j] = shader.vertex(i, j);
            }
            triangle(screen_coords, shader, frame, zbuffer);
        }
        frame.flip_vertically(); // to place the origin in the bottom left corner of the image
        frame.write_tga_file("framebuffer.tga");
    }

    delete model;
    delete [] zbuffer;
    delete [] shadowbuffer;
    return 0;
}

