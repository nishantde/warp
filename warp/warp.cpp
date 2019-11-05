//   Author: Ioannis Karamouzas Date: 09/15/2018
//   Sample Solution to Homework Assignment 1: Get the Picture!
//   OpenGL/GLUT Program using OpenImageIO to Read and Write Images, and OpenGL to display
//
//   The program responds to the following keyboard commands:
//    r or R: prompt for an input image file name, read the image into
//	    an appropriately sized pixmap, resize the window, and display
//
//    w or W: prompt for an output image file name, read the display into a pixmap,
//	    and write from the pixmap to the file.
//
//    q, Q or ESC: quit.
//
//   When the window is resized by the user: If the size of the window becomes bigger than
//   the image, the image is centered in the window. If the size of the window becomes
//   smaller than the image, the image is uniformly scaled down to the largest size that
//   fits in the window.
//


#include <OpenImageIO/imageio.h>
#include <iostream>
#include <GL/glut.h>
#include "matrix.h"
#include <cstdlib>
#include <string>
#include <math.h>

using namespace std;
using std::string;
OIIO_NAMESPACE_USING


struct Pixel{ // defines a pixel structure
	unsigned char r,g,b,a;
};

//
// Global variables and constants
//
const int DEFAULTWIDTH = 600;	// default window dimensions if no image
const int DEFAULTHEIGHT = 600;

int WinWidth, WinHeight;	// window width and height
int ImWidth, ImHeight;		// image width and height
int ImChannels;           // number of channels per image pixel

int VpWidth, VpHeight;		// viewport width and height
int Xoffset, Yoffset;     // viewport offset from lower left corner of window

Pixel **pixmap = NULL;	// the image pixmap used for OpenGL
Pixel **dupPixmap = NULL;
bool isWarpPerformed = false;
string imageFileName;
Matrix3D M;

int outputWidth, outputHeight;
int pixformat; 			// the pixel format used to correctly  draw the image

//
//  Routine to cleanup the memory. Note that GLUT and OpenImageIO v1.6 lead to memory leaks that cannot be suppressed.
//
void destroy(){
 if (pixmap){
     delete pixmap[0];
	 delete pixmap;
  }
}

//
//  Routine to read an image file and store in a pixmap
//  returns the size of the image in pixels if correctly read, or 0 if failure
//
int readimage(string infilename){
  // Create the oiio file handler for the image, and open the file for reading the image.
  // Once open, the file spec will indicate the width, height and number of channels.
  ImageInput *infile = ImageInput::open(infilename);
  if(!infile){
    cerr << "Could not input image file " << infilename << ", error = " << geterror() << endl;
    return 0;
  }

  // Record image width, height and number of channels in global variables
  ImWidth = infile->spec().width;
	cout << "Image Width: " << ImWidth << "\n";
  ImHeight = infile->spec().height;
	cout << "Image Height: " << ImHeight << "\n";
  ImChannels = infile->spec().nchannels;
	cout << "Image Channels: " << ImChannels << "\n";

	outputWidth = ImWidth;
	outputHeight = ImHeight;


  // allocate temporary structure to read the image
  unsigned char tmp_pixels[ImWidth * ImHeight * ImChannels];

  // read the image into the tmp_pixels from the input file, flipping it upside down using negative y-stride,
  // since OpenGL pixmaps have the bottom scanline first, and
  // oiio expects the top scanline first in the image file.
  int scanlinesize = ImWidth * ImChannels * sizeof(unsigned char);
  if(!infile->read_image(TypeDesc::UINT8, &tmp_pixels[0] + (ImHeight - 1) * scanlinesize, AutoStride, -scanlinesize)){
    cerr << "Could not read image from " << infilename << ", error = " << geterror() << endl;
    ImageInput::destroy(infile);
    return 0;
  }

 // get rid of the old OpenGL pixmap and make a new one of the new size
  destroy();

 // allocate space for the Pixmap (contiguous approach)
  pixmap = new Pixel*[ImHeight];
  if(pixmap != NULL)
	pixmap[0] = new Pixel[ImWidth * ImHeight];
  for(int i = 1; i < ImHeight; i++)
	pixmap[i] = pixmap[i - 1] + ImWidth;

 //  assign the read pixels to the Pixmap
 int index;
  for(int row = 0; row < ImHeight; ++row) {
    for(int col = 0; col < ImWidth; ++col) {
		index = (row*ImWidth+col)*ImChannels;

		if (ImChannels==1){
			pixmap[row][col].r = tmp_pixels[index];
			pixmap[row][col].g = tmp_pixels[index];
			pixmap[row][col].b = tmp_pixels[index];
			pixmap[row][col].a = 255;
		}
		else{
			pixmap[row][col].r = tmp_pixels[index];
			pixmap[row][col].g = tmp_pixels[index+1];
			pixmap[row][col].b = tmp_pixels[index+2];
			if (ImChannels <4) // no alpha value is present so set it to 255
				pixmap[row][col].a = 255;
			else // read the alpha value
				pixmap[row][col].a = tmp_pixels[index+3];
		}
    }
  }

  // close the image file after reading, and free up space for the oiio file handler
  infile->close();
  ImageInput::destroy(infile);

  // set the pixel format to GL_RGBA and fix the # channels to 4
  pixformat = GL_RGBA;
  ImChannels = 4;

  // return image size in pixels
  return ImWidth * ImHeight;
}

void Rotate(Matrix3D &M, float theta) {
	Matrix3D R;
	double rad, c, s;
	rad = M_PI * theta / 180.0;
	c = cos(rad);
	s = sin(rad);
	R[0][0] = c;
	R[0][1] = s;
	R[1][0] = -s;
	R[1][1] = c;

	M = R * M;
}

void Scale(Matrix3D &M, float xScale, float yScale) {
	Matrix3D S;
	S[0][0] = xScale;
	S[1][1] = yScale;

	M = S * M;
}

void Translate(Matrix3D &M, float xTranslate, float yTranslate) {
	Matrix3D T;
	T[0][2] = xTranslate;
	T[1][2] = yTranslate;

	M = T * M;
}

void Shear(Matrix3D &M, float xShear, float yShear) {
	Matrix3D H;
	H[0][1] = xShear;
	H[1][0] = yShear;

	M = H * M;
}

void Flip(Matrix3D &M, int xFlip, int yFlip) {
	float temp;
	if (xFlip == 1) {
		for(int i=0; i<3; ++i) {
			temp = M[i][0];
			M[i][0] = M[i][2];
			M[i][2] = temp;
		}
	}

	if (yFlip == 1) {
		for(int j=0; j<3; ++j) {
			temp = M[0][j];
			M[0][j] = M[2][j];
			M[2][j] = temp;
		}
	}
}

void Perspective(Matrix3D &M, float xPerspective, float yPerspective) {
	Matrix3D P;
	P[2][0] = xPerspective;
	P[2][1] = yPerspective;

	M = P * M;
}

void read_input(Matrix3D &M) {
	string cmd;
	/* prompt for user input */
	do
	{
			cout << "> ";
			cin >> cmd;
			if(cmd.length() != 1) {
				cout << "Invalid command! Enter 'r', 's', 't', 'h', 'f', 'p', or 'd'\n";
			}

			else {
				switch(cmd[0]) {
					case 'r':
						float theta;
						cout << "Theta: ";
						cin >> theta;
						if (cin) {
							Rotate(M, theta);
							cout << "Rotation done\n";
						}
						else {
							cerr < "Invalid rotation angle\n";
							cin.clear();
						}
						break;

					case 's':
						float sx, sy;
						cout << "X Scale: ";
						cin >> sx;
						cout << "Y Scale: ";
						cin >> sy;
						Scale(M, sx, sy);
						break;

					case 't':
						float tx, ty;
						cout << "X Translate: ";
						cin >> tx;
						cout << "Y Translate: ";
						cin >> ty;
						Translate(M, tx, ty);
						break;

					case 'h':
						float hx, hy;
						cout << "X Shear: ";
						cin >> hx;
						cout << "Y Shear: ";
						cin >> hy;
						Shear(M, hx, hy);
						break;

					case 'f':
					int fx, fy;
					cout << "X Flip Flag (0/1): ";
					cin >> fx;
					cout << "Y Flip Flag (0/1): ";
					cin >> fy;
					Flip(M, fx, fy);
					break;

					case 'p':
						float px, py;
						cout << "X Perspective: ";
						cin >> px;
						cout << "Y Perspective: ";
						cin >> py;
						Perspective(M, px, py);
						break;

					case 'm':
						cout << "Accumulated Matrix:" << "\n";
						M.print();
						break;

					case 'd':
						break;

					default:
						cout << "Invalid command! Enter 'r', 's', 't', 'h', 'f', 'p', or 'd'\n";
				}
			}
	} while (cmd.compare("d") != 0);
}

Vector3D transformAndNormalize(Matrix3D &M, Vector3D &coordinate) {
	//cout << "\nBefore Multiplication: " << coordinate.x << " " << coordinate.y << " " << coordinate.z << "\n";
	coordinate = M * coordinate;
	//cout << "After Multiplication: " << coordinate.x << " " << coordinate.y << " " << coordinate.z << "\n";
	coordinate.x /= coordinate.z;
	coordinate.y /= coordinate.z;
	coordinate.z = 1;
	//cout << "After Normalization: " << coordinate.x << " " << coordinate.y << " " << coordinate.z << "\n";
	return coordinate;
}

void performWarp(Matrix3D &M, int &outputWidth, int &outputHeight, string imageFileName) {
	int leftValue, rightValue;
	int topValue, bottomValue;
	int u, v;

	Vector3D top_left = Vector3D(0, ImHeight, 1);
	Vector3D top_right = Vector3D(ImWidth, ImHeight, 1);
	Vector3D bottom_left = Vector3D(0, 0, 1);
	Vector3D bottom_right = Vector3D(ImWidth, 0, 1);

	top_left = transformAndNormalize(M, top_left);
	cout << "Before -> top_right.x: " << top_right.x << " y: " << top_right.y << " z: " << top_right.z << "\n";
	top_right = transformAndNormalize(M, top_right);
	cout << "After -> top_right.x: " << top_right.x << " y: " << top_right.y << " z: " << top_right.z << "\n";
	bottom_left = transformAndNormalize(M, bottom_left);
	bottom_right = transformAndNormalize(M, bottom_right);

	leftValue = ceil(min(top_left.x, min(top_right.x, min(bottom_left.x, bottom_right.x))));
	bottomValue = ceil(min(top_left.y, min(top_right.y, min(bottom_left.y, bottom_right.y))));
	rightValue = ceil(max(top_left.x, max(top_right.x, max(bottom_left.x, bottom_right.x))));
	topValue = ceil(max(top_left.y, min(top_right.y, min(bottom_left.y, bottom_right.y))));


	outputWidth = rightValue - leftValue;
	outputHeight = topValue - bottomValue;
	cout << "Output Width: " << outputWidth << "\n";
	cout << "Output Height: " << outputHeight << "\n";

	Matrix3D O;
	O[0][2] = leftValue;
	O[1][2] = bottomValue;

	Matrix3D Inv = M.inverse();
	Inv = Inv * O;

	dupPixmap = new Pixel*[outputHeight];

	if(dupPixmap != NULL){
		dupPixmap[0] = new Pixel[outputWidth * outputHeight];
	}

	for(int i = 1; i < outputHeight; i++){
		dupPixmap[i] = dupPixmap[i - 1] + outputWidth;
	}

	for (int row=0; row<outputHeight; ++row) {
		for(int col=0; col<outputWidth; ++col) {
			cout << "row: " << row << " col: " << col << "\n";
			Vector3D outputPixel(col, row, 1);

			outputPixel = transformAndNormalize(Inv, outputPixel);
			u = outputPixel.x;
			v = outputPixel.y;

			if (u < 1 || u > outputWidth - 1) {
				dupPixmap[row][col].r = 0;
				dupPixmap[row][col].g = 0;
				dupPixmap[row][col].b = 0;
				dupPixmap[row][col].a = 255;
			}

			else if (v < 1 || v > outputHeight - 1) {
				dupPixmap[row][col].r = 0;
				dupPixmap[row][col].g = 0;
				dupPixmap[row][col].b = 0;
				dupPixmap[row][col].a = 255;
			}

			else {
				dupPixmap[row][col].r = pixmap[v][u].r;
				dupPixmap[row][col].g = pixmap[v][u].g;
				dupPixmap[row][col].b = pixmap[v][u].b;
				dupPixmap[row][col].a = pixmap[v][u].a;
			}

		}
	}
}

//
// Routine to display a pixmap in the current window
//
void displayimage(){
  // if the window is smaller than the image, scale it down, otherwise do not scale
  if(WinWidth < ImWidth  || WinHeight < ImHeight)
    glPixelZoom(float(VpWidth) / ImWidth, float(VpHeight) / ImHeight);
  else
    glPixelZoom(1.0, 1.0);

  // display starting at the lower lefthand corner of the viewport
  glRasterPos2i(0, 0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glDrawPixels(outputWidth, outputHeight, pixformat, GL_UNSIGNED_BYTE, dupPixmap[0]);
}

//
// Routine to write the current framebuffer to an image file
//
void writeimage(string outfilename){
  // make a pixmap that is the size of the window and grab OpenGL framebuffer into it
   unsigned char local_pixmap[WinWidth * WinHeight * ImChannels];
   glReadPixels(0, 0, WinWidth, WinHeight, pixformat, GL_UNSIGNED_BYTE, local_pixmap);

  // create the oiio file handler for the image
  ImageOutput *outfile = ImageOutput::create(outfilename);
  if(!outfile){
    cerr << "Could not create output image for " << outfilename << ", error = " << geterror() << endl;
    return;
  }

  // Open a file for writing the image. The file header will indicate an image of
  // width WinWidth, height WinHeight, and ImChannels channels per pixel.
  // All channels will be of type unsigned char
  ImageSpec spec(WinWidth, WinHeight, ImChannels, TypeDesc::UINT8);
  if(!outfile->open(outfilename, spec)){
    cerr << "Could not open " << outfilename << ", error = " << geterror() << endl;
    ImageOutput::destroy(outfile);
    return;
  }

  // Write the image to the file. All channel values in the pixmap are taken to be
  // unsigned chars. While writing, flip the image upside down by using negative y stride,
  // since OpenGL pixmaps have the bottom scanline first, and oiio writes the top scanline first in the image file.
  int scanlinesize = WinWidth * ImChannels * sizeof(unsigned char);
  if(!outfile->write_image(TypeDesc::UINT8, local_pixmap + (WinHeight - 1) * scanlinesize, AutoStride, -scanlinesize)){
    cerr << "Could not write image to " << outfilename << ", error = " << geterror() << endl;
    ImageOutput::destroy(outfile);
    return;
  }

  // close the image file after the image is written and free up space for the
  // ooio file handler
  outfile->close();
  ImageOutput::destroy(outfile);
}

//
//   Display Callback Routine: clear the screen and draw the current image
//
void handleDisplay(){

  // specify window clear (background) color to be opaque black
  glClearColor(0, 0, 0, 1);
  // clear window to background color
  glClear(GL_COLOR_BUFFER_BIT);

  // only draw the image if it is of a valid size
  if(outputWidth > 0 && outputHeight > 0)
    displayimage();

  // flush the OpenGL pipeline to the viewport
  glFlush();
}

//
//  Keyboard Callback Routine: 'r' - read and display a new image,
//  'w' - write the current window to an image file, 'q' or ESC - quit
//
void handleKey(unsigned char key, int x, int y){
  string infilename, outfilename;
  int ok;

  switch(key){
    case 'r':		// 'r' - read an image from a file
    case 'R':
      cout << "Input image filename? ";	  // prompt user for input filename
      cin >> infilename;
      ok = readimage(infilename);
      if(ok)
        glutReshapeWindow(ImWidth, ImHeight); // OpenGL window should match new image
      glutPostRedisplay();
      break;

    case 'w':		// 'w' - write the image to a file
    case 'W':
      cout << "Output image filename? ";  // prompt user for output filename
      cin >> outfilename;
      writeimage(outfilename);
      break;

    case 'q':		// q or ESC - quit
    case 'Q':
    case 27:
      destroy();
      exit(0);

    default:		// not a valid key -- just ignore it
      return;
  }
}

//
//  Reshape Callback Routine: If the window is too small to fit the image,
//  make a viewport of the maximum size that maintains the image proportions.
//  Otherwise, size the viewport to match the image size. In either case, the
//  viewport is centered in the window.
//
void handleReshape(int w, int h){
  float imageaspect = (float)ImWidth / (float)ImHeight;	// aspect ratio of image
  float newaspect = (float)w / (float)h; // new aspect ratio of window

  // record the new window size in global variables for easy access
  WinWidth = w;
  WinHeight = h;

  // if the image fits in the window, viewport is the same size as the image
  if(w >= ImWidth && h >= ImHeight){
    Xoffset = (w - ImWidth) / 2;
    Yoffset = (h - ImHeight) / 2;
    VpWidth = ImWidth;
    VpHeight = ImHeight;
  }
  // if the window is wider than the image, use the full window height
  // and size the width to match the image aspect ratio
  else if(newaspect > imageaspect){
    VpHeight = h;
    VpWidth = int(imageaspect * VpHeight);
    Xoffset = int((w - VpWidth) / 2);
    Yoffset = 0;
  }
  // if the window is narrower than the image, use the full window width
  // and size the height to match the image aspect ratio
  else{
    VpWidth = w;
    VpHeight = int(VpWidth / imageaspect);
    Yoffset = int((h - VpHeight) / 2);
    Xoffset = 0;
  }

  // center the viewport in the window
  glViewport(Xoffset, Yoffset, VpWidth, VpHeight);

  // viewport coordinates are simply pixel coordinates
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, VpWidth, 0, VpHeight);
  glMatrixMode(GL_MODELVIEW);
}


//
// Main program to scan the commandline, set up GLUT and OpenGL, and start Main Loop
//
int main(int argc, char* argv[]){
	// start up GLUT
	glutInit(&argc, argv);

	// create the graphics window, giving width, height, and title text
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
	glutInitWindowSize(WinWidth, WinHeight);
	glutCreateWindow("Warping");

	// set up the callback routines
	glutDisplayFunc(handleDisplay); // display update callback
	glutKeyboardFunc(handleKey);	  // keyboard key press callback
	glutReshapeFunc(handleReshape); // window resize callback


	// Enter GLUT's event loop
	read_input(M);

	imageFileName = "dhouse.png";
	readimage(imageFileName);

	performWarp(M, ImWidth, ImHeight, imageFileName);
	glutReshapeWindow(outputWidth, outputHeight);
	glutPostRedisplay();

	cout << "Accumulated Matrix: " << "\n";
	M.print();

  // scan command line and process
  // only one parameter allowed, an optional image filename and extension
  if(argc > 3){
    cout << "usage: ./warper <inputImage.ext> [outputImage.ext]" << endl;
    exit(1);
  }

  // set up the default window and empty pixmap if no image or image fails to load
  WinWidth = DEFAULTWIDTH;
  WinHeight = DEFAULTHEIGHT;
  ImWidth = 0;
  ImHeight = 0;

  // load the image if present, and size the window to match
  if(argc == 2){
    if(readimage(argv[1])){
      WinWidth = ImWidth;
      WinHeight = ImHeight;
    }
  }


  glutMainLoop();
  return 0;
}
