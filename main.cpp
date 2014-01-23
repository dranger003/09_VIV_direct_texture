/*
* Copyright (c) 2012 Freescale Semiconductor, Inc.
*/

/****************************************************************************
*
*    Copyright 2012 Vivante Corporation, Sunnyvale, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/


/*
* OpenGL ES 2.0 Tutorial
*
* Draws a simple triangle with basic vertex and pixel shaders.
*/
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES	1
#endif

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "FSL/fsl_egl.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <time.h>

#ifndef UNDER_CE
#include <sys/timeb.h>
#include <fcntl.h>
#endif
#include <string.h>

#define TUTORIAL_NAME "OpenGL ES 2.0 Tutorial 5"

EGLDisplay			egldisplay;
EGLConfig			eglconfig;
EGLSurface			eglsurface;
EGLContext			eglcontext;
EGLNativeWindowType eglNativeWindow;
EGLNativeDisplayType eglNativeDisplayType;
volatile sig_atomic_t quit = 0;

/* GL_VIV_direct_texture */
#ifndef GL_VIV_direct_texture
#define GL_VIV_YV12                     0x8FC0
#define GL_VIV_NV12                     0x8FC1
#define GL_VIV_YUY2                     0x8FC2
#define GL_VIV_UYVY                     0x8FC3
#define GL_VIV_NV21                     0x8FC4
#endif

typedef void (GL_APIENTRY *PFNGLTEXDIRECTVIV) (GLenum Target, GLsizei Width, GLsizei Height, GLenum Format, GLvoid ** Pixels);
typedef void (GL_APIENTRY *PFNGLTEXDIRECTINVALIDATEVIV) (GLenum Target);
static PFNGLTEXDIRECTVIV pFNglTexDirectVIV = NULL;
static PFNGLTEXDIRECTINVALIDATEVIV pFNglTexDirectInvalidateVIV = NULL;

// Wrapper to load vetex and pixel shader.
void LoadShaders(const char * vShaderFName, const char * pShaderFName);
// Cleanup the shaders.
void DestroyShaders();

// Global Variables, attribute and uniform
GLint locVertices     = 0;
GLint locColors       = 0;
GLint locTransformMat = 0;
GLint locTexcoord	  = 0;
GLint locSampler = 0;
GLuint gTexObj = 0;

// Global Variables, shader handle and program handle
GLuint vertShaderNum  = 0;
GLuint pixelShaderNum = 0;
GLuint programHandle  = 0;

// Triangle Vertex positions.
const GLfloat vertices[][2] = {
	{ -0.5f, -0.5f},
	{  0.5f, -0.5f},
	{ -0.5f, 0.5f},
	{  0.5f,  0.5f}
};
const GLfloat texcoords[][2] = {
	{ 0.0f, 1.0f},
	{ 1.0f, 1.0f},
	{ 0.0f, 0.0f},
	{ 1.0f, 0.0f}
};

// Triangle Vertex colors.
const GLfloat color[][3] = {
	{1.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, 1.0f},
	{1.0f, 1.0f, 0.0f}
};

// Start with an identity matrix.
GLfloat transformMatrix[16] =
{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

/***************************************************************************************
***************************************************************************************/
void *planes[3];
int vFrames = 0;
FILE *file = NULL;
long long fileSize = 0;
int frameSize = 0, ySize = 0, uSize = 0, vSize = 0;

int width = 0, height = 0;

int LoadFrame()
{
	if (feof(file))
	{
		fseek(file, fileSize % frameSize, SEEK_SET);
		return 0;
	}

	if (fread(planes[0], ySize, 1, file) <= 0)
		return 0;

	if (vSize > 0)
	{
		if (fread(planes[1], vSize, 1, file) <= 0)
			return 0;
	}

	if (uSize > 0)
	{
		if (fread(planes[2], uSize, 1, file) <= 0)
			return 0;
	}

	/* Mark as dirty. */
	(*pFNglTexDirectInvalidateVIV)(GL_TEXTURE_2D);
	return 0;
}

GLuint Load420Texture(
					  const char* FileName,
					  int Width,
					  int Height,
					  int format
					  )
{
	GLuint result = 0;
	do
	{
		GLuint name;
#ifdef UNDER_CE
		static wchar_t buffer[MAX_PATH + 1];
		int i = GetModuleFileName(NULL, buffer, MAX_PATH);
		while (buffer[i - 1] != L'\\') i--;
		while (*FileName != '\0') buffer[i++] = (wchar_t)(*FileName++);
		buffer[i] = L'\0';
		file = _wfopen(buffer, L"rb");
#else
		/* Open the texture file. */
		file = fopen(FileName, "rb");
#endif

		if (file == NULL)
		{
			break;
		}

		/* Determine the size of the file. */
		if (fseek(file, 0, SEEK_END) == -1)
		{
			break;
		}

		switch (format)
		{
		case GL_VIV_YV12:
			ySize     = Width * Height;
			vSize     = ySize / 4;
			uSize     = vSize;
			break;

		case GL_VIV_NV12:
		case GL_VIV_NV21:
			ySize     = Width * Height;
			vSize     = ySize / 2;
			uSize     = 0;
			break;

		case GL_VIV_YUY2:
		case GL_VIV_UYVY:
			ySize     = 2 * Width * Height;
			vSize     = 0;
			uSize     = 0;
			break;
			return 0;
		}

		frameSize = ySize + uSize + vSize;
		fileSize  = ftell(file);
		vFrames   = fileSize/frameSize;		

		/* Determine the number of frames in the file. */
		if ((fileSize <= 0) || (frameSize <= 0))
		{
			break;
		}

		/* Create the texture. */
		glGenTextures(1, &name);
		glBindTexture(GL_TEXTURE_2D, name);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		(*pFNglTexDirectVIV)(
			GL_TEXTURE_2D,
			Width,
			Height,
			format,
			(GLvoid**) &planes
			);
		if (glGetError() != GL_NO_ERROR)
		{
			break;
		}

		fseek(file, fileSize % frameSize, SEEK_SET);

		if (fread(planes[0], ySize, 1, file) <= 0)
			return 0;

		if (vSize > 0)
		{
			if (fread(planes[1], vSize, 1, file) <= 0)
				return 0;
		}

		if (uSize > 0)
		{
			if (fread(planes[2], uSize, 1, file) <= 0)
				return 0;
		}

		/* Mark as dirty. */
		(*pFNglTexDirectInvalidateVIV)(GL_TEXTURE_2D);

		/* Success. */
		result = name;
	}
	while (0);
	/* Return result. */
	return result;
}

bool RenderInit()
{
	if (pFNglTexDirectVIV == NULL)
	{
		/* Get the pointer to the glTexDirectVIV function. */
		pFNglTexDirectVIV = (PFNGLTEXDIRECTVIV)
			eglGetProcAddress("glTexDirectVIV");

		if (pFNglTexDirectVIV == NULL)
		{
			printf("Required extension not supported.\n");
			/* The glTexDirectVIV function is not available. */
			return false;
		}
	}

	if (pFNglTexDirectInvalidateVIV == NULL) {
		/* Get the pointer to the glTexDirectInvalidate function. */
		pFNglTexDirectInvalidateVIV = (PFNGLTEXDIRECTINVALIDATEVIV)
			eglGetProcAddress("glTexDirectInvalidateVIV");

		if (pFNglTexDirectInvalidateVIV == NULL)
		{
			printf("Required extension not supported.\n");
			/* The glTexDirectInvalidateVIV function is not available. */
			return false;
		}
	}

	// Grab location of shader attributes.
	locVertices = glGetAttribLocation(programHandle, "my_Vertex");
	locColors   = glGetAttribLocation(programHandle, "my_Color");
	locTexcoord = glGetAttribLocation(programHandle, "my_Texcoor");
	// Transform Matrix is uniform for all vertices here.
	locTransformMat = glGetUniformLocation(programHandle, "my_TransformMatrix");
	locSampler = glGetUniformLocation(programHandle, "my_Sampler");

#ifndef ANDROID_JNI
	gTexObj = Load420Texture("f430_160x120xNV21.yuv", 160, 120, GL_VIV_NV21);
#else
	gTexObj = Load420Texture("/sdcard/tutorial/tutorial5/f430_160x120xNV21.yuv", 160, 120, GL_VIV_NV21);
#endif

	if (gTexObj == 0)
	{
		printf("Could not load the texture file: f430_160x120xNV21.yuv. Exiting.\n");
		return false;
	}

	// enable vertex arrays to push the data.
	glEnableVertexAttribArray(locVertices);
	glEnableVertexAttribArray(locColors);
	glEnableVertexAttribArray(locTexcoord);

	// set data in the arrays.
	glVertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, &vertices[0][0]);
	glVertexAttribPointer(locColors, 3, GL_FLOAT, GL_FALSE, 0, &color[0][0]);
	glVertexAttribPointer(locTexcoord, 2, GL_FLOAT, GL_FALSE, 0, &texcoords[0][0]);
	glUniformMatrix4fv(locTransformMat, 1, GL_FALSE, transformMatrix);
	glUniform1i(locSampler, 0);// gTexObj);

	return (GL_NO_ERROR == glGetError());
}

// Actual rendering here.
void Render()
{
	static float angle = 0;

	// Clear background.
	glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up rotation matrix rotating by angle around y axis.
	transformMatrix[0] = transformMatrix[10] = (GLfloat)cos(angle);
	transformMatrix[2] = (GLfloat)sin(angle);
	transformMatrix[8] = -transformMatrix[2];
	angle += 0.1f;
	glUniformMatrix4fv(locTransformMat, 1, GL_FALSE, transformMatrix);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// flush all commands.
	glFlush ();

	LoadFrame();
}

void RenderCleanup()
{
	// cleanup
	glDisableVertexAttribArray(locVertices);
	glDisableVertexAttribArray(locColors);
	glDisableVertexAttribArray(locTexcoord);
	DestroyShaders();

	if (file != NULL) {
		fclose(file);
		file = NULL;
	}
}

/***************************************************************************************
***************************************************************************************/

// Compile a vertex or pixel shader.
// returns 0: fail
//         1: success
int CompileShader(const char * FName, GLuint ShaderNum)
{
	FILE * fptr = NULL;
#ifdef UNDER_CE
	static wchar_t buffer[MAX_PATH + 1];
	int i = GetModuleFileName(NULL, buffer, MAX_PATH);
	while (buffer[i - 1] != L'\\') i--;
	while (*FName != '\0') buffer[i++] = (wchar_t)(*FName++);
	buffer[i] = L'\0';
	fptr = _wfopen(buffer, L"rb");
#else
	fptr = fopen(FName, "rb");
#endif
	if (fptr == NULL)
	{
		return 0;
	}

	int length;
	fseek(fptr, 0, SEEK_END);
	length = ftell(fptr);
	fseek(fptr, 0 ,SEEK_SET);

	char * shaderSource = (char*)malloc(sizeof (char) * length);
	if (shaderSource == NULL)
	{
		fprintf(stderr, "Out of memory.\n");
		return 0;
	}

	fread(shaderSource, length, 1, fptr);

	glShaderSource(ShaderNum, 1, (const char**)&shaderSource, &length);
	glCompileShader(ShaderNum);

	free(shaderSource);
	fclose(fptr);

	GLint compiled = 0;
	glGetShaderiv(ShaderNum, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		// Retrieve error buffer size.
		GLint errorBufSize, errorLength;
		glGetShaderiv(ShaderNum, GL_INFO_LOG_LENGTH, &errorBufSize);

		char * infoLog = (char*)malloc(errorBufSize * sizeof(char) + 1);
		if (infoLog)
		{
			// Retrieve error.
			glGetShaderInfoLog(ShaderNum, errorBufSize, &errorLength, infoLog);
			infoLog[errorBufSize] = '\0';
			fprintf(stderr, "%s\n", infoLog);

			free(infoLog);
		}
		return 0;
	}

	return 1;
}

/***************************************************************************************
***************************************************************************************/

// Wrapper to load vetex and pixel shader.
void LoadShaders(const char * vShaderFName, const char * pShaderFName)
{
	vertShaderNum = glCreateShader(GL_VERTEX_SHADER);
	pixelShaderNum = glCreateShader(GL_FRAGMENT_SHADER);

	if (CompileShader(vShaderFName, vertShaderNum) == 0)
	{
		printf("%d: PS compile failed.\n", __LINE__);
		return;
	}

	if (CompileShader(pShaderFName, pixelShaderNum) == 0)
	{
		printf("%d: VS compile failed.\n", __LINE__);
		return;
	}

	programHandle = glCreateProgram();

	glAttachShader(programHandle, vertShaderNum);
	glAttachShader(programHandle, pixelShaderNum);

	glLinkProgram(programHandle);
	// Check if linking succeeded.
	GLint linked = false;
	glGetProgramiv(programHandle, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		printf("%d: Link failed.\n", __LINE__);
		// Retrieve error buffer size.
		GLint errorBufSize, errorLength;
		glGetShaderiv(programHandle, GL_INFO_LOG_LENGTH, &errorBufSize);

		char * infoLog = (char*)malloc(errorBufSize * sizeof (char) + 1);
		if (!infoLog)
		{
			// Retrieve error.
			glGetProgramInfoLog(programHandle, errorBufSize, &errorLength, infoLog);
			infoLog[errorBufSize + 1] = '\0';
			fprintf(stderr, "%s", infoLog);

			free(infoLog);
		}

		return;
	}
	glUseProgram(programHandle);
}

// Cleanup the shaders.
void DestroyShaders()
{
	if (programHandle) {
		glDeleteShader(vertShaderNum);
		glDeleteShader(pixelShaderNum);
		glDeleteProgram(programHandle);
		glUseProgram(0);
		programHandle = 0;
	}
}


/***************************************************************************************
***************************************************************************************/
int eglInit(void)
{	
	static const EGLint s_configAttribs[] =
	{
		EGL_SAMPLES,      0,
		EGL_RED_SIZE,     8,
		EGL_GREEN_SIZE,   8,
		EGL_BLUE_SIZE,    8,
		EGL_ALPHA_SIZE,   EGL_DONT_CARE,
		EGL_DEPTH_SIZE,   0,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE,
	};

	EGLint numconfigs;

	eglNativeDisplayType = fsl_getNativeDisplay();

	fbGetDisplayGeometry(eglNativeDisplayType, &width, &height);
	printf("%dx%d\n", width, height);

	egldisplay = eglGetDisplay(eglNativeDisplayType);
	eglInitialize(egldisplay, NULL, NULL);
	assert(eglGetError() == EGL_SUCCESS);
	eglBindAPI(EGL_OPENGL_ES_API);

	eglChooseConfig(egldisplay, s_configAttribs, &eglconfig, 1, &numconfigs);
	assert(eglGetError() == EGL_SUCCESS);
	assert(numconfigs == 1);

	eglNativeWindow = fsl_createwindow(egldisplay, eglNativeDisplayType);	
	assert(eglNativeWindow);	

	eglsurface = eglCreateWindowSurface(egldisplay, eglconfig, eglNativeWindow, NULL);

	assert(eglGetError() == EGL_SUCCESS);
	EGLint ContextAttribList[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

	eglcontext = eglCreateContext( egldisplay, eglconfig, EGL_NO_CONTEXT, ContextAttribList );
	assert(eglGetError() == EGL_SUCCESS);
	eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglcontext);
	assert(eglGetError() == EGL_SUCCESS);

	return 1;
}

void eglDeinit(void)
{
	printf("Cleaning up...\n");
	eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	assert(eglGetError() == EGL_SUCCESS);
	eglDestroyContext(egldisplay, eglcontext);
	eglDestroySurface(egldisplay, eglsurface);	
	fsl_destroywindow(eglNativeWindow, eglNativeDisplayType);
	eglTerminate(egldisplay);
	assert(eglGetError() == EGL_SUCCESS);
	eglReleaseThread();	
}

void sighandler(int signal)
{
	printf("Caught signal %d, setting flaq to quit.\n", signal);
	quit = 1;
}

/***************************************************************************************
***************************************************************************************/

// Program entry.
int main(int argc, char** argv)
{
	// Enable vsync
	setenv("FB_MULTI_BUFFER", "2", 0);

	system("echo 0 > /sys/devices/virtual/graphics/fbcon/cursor_blink");

	int	currentFrame = 0;
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);   
	assert( eglInit() );   
	// load and compiler vertex/fragment shaders.
	LoadShaders("vs_es20t5.vert", "ps_es20t5.frag");
	if (programHandle != 0)
	{
		if (!RenderInit())
		{
			goto OnError;
		}

		system("setterm -cursor off");

		struct timespec abeg, cbeg, end;
		clock_gettime(CLOCK_MONOTONIC, &abeg);

		while (!quit)
		{
			clock_gettime(CLOCK_MONOTONIC, &cbeg);

			Render();
			currentFrame++;
			eglSwapBuffers(egldisplay, eglsurface);

			clock_gettime(CLOCK_MONOTONIC, &end);

			printf("\ravg = %.6Lf fps, cur = %.6Lf fps                       \r",
				(long double)currentFrame / ((end.tv_sec - abeg.tv_sec) + (end.tv_nsec - abeg.tv_nsec) / 1000000000.0),
				1.0L / ((end.tv_sec - cbeg.tv_sec) + (end.tv_nsec - cbeg.tv_nsec) / 1000000000.0));
		}

		system("setterm -cursor on");

		printf("\n");

		glFinish();
		/*unsigned int end = vdkGetTicks();
		float fps = frameCount / ((end - start) / 1000.0f);
		printf("%d frames in %d ticks -> %.3f fps\n", frameCount, end - start, fps);*/

		RenderCleanup();
	}

OnError:

	DestroyShaders();

	if (file != NULL) {
		fclose(file);
	}
	eglDeinit();

	char cmdline[32];
	sprintf(cmdline, "fbset -xres %d -yres %d", width, height);
	system(cmdline);

	system("echo 1 > /sys/devices/virtual/graphics/fbcon/cursor_blink");

	return 0;
}
