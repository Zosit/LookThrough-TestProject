#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct Point
{
	float x0, y0, z0;       // initial coordinates
	float x, y, z;        // animated coordinates
};

struct Curve
{
	float r, g, b;
	Point p0, p1, p2, p3;
};

//#include <bmptotexture.cpp>
//-------------------------
int	ReadInt(FILE *);
short	ReadShort(FILE *);


struct bmfh
{
	short bfType;
	int bfSize;
	short bfReserved1;
	short bfReserved2;
	int bfOffBits;
} FileHeader;

struct bmih
{
	int biSize;
	int biWidth;
	int biHeight;
	short biPlanes;
	short biBitCount;
	int biCompression;
	int biSizeImage;
	int biXPelsPerMeter;
	int biYPelsPerMeter;
	int biClrUsed;
	int biClrImportant;
} InfoHeader;

const int birgb = { 0 };



/**
** read a BMP file into a Texture:
**/

unsigned char *
BmpToTexture(char *filename, int *width, int *height)
{

	int s, t, e;		// counters
	int numextra;		// # extra bytes each line in the file is padded with
	FILE *fp;
	unsigned char *texture;
	int nums, numt;
	unsigned char *tp;


	fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "Cannot open Bmp file '%s'\n", filename);
		return NULL;
	}

	FileHeader.bfType = ReadShort(fp);


	// if bfType is not 0x4d42, the file is not a bmp:

	if (FileHeader.bfType != 0x4d42)
	{
		fprintf(stderr, "Wrong type of file: 0x%0x\n", FileHeader.bfType);
		fclose(fp);
		return NULL;
	}


	FileHeader.bfSize = ReadInt(fp);
	FileHeader.bfReserved1 = ReadShort(fp);
	FileHeader.bfReserved2 = ReadShort(fp);
	FileHeader.bfOffBits = ReadInt(fp);


	InfoHeader.biSize = ReadInt(fp);
	InfoHeader.biWidth = ReadInt(fp);
	InfoHeader.biHeight = ReadInt(fp);

	nums = InfoHeader.biWidth;
	numt = InfoHeader.biHeight;

	InfoHeader.biPlanes = ReadShort(fp);
	InfoHeader.biBitCount = ReadShort(fp);
	InfoHeader.biCompression = ReadInt(fp);
	InfoHeader.biSizeImage = ReadInt(fp);
	InfoHeader.biXPelsPerMeter = ReadInt(fp);
	InfoHeader.biYPelsPerMeter = ReadInt(fp);
	InfoHeader.biClrUsed = ReadInt(fp);
	InfoHeader.biClrImportant = ReadInt(fp);


	// fprintf( stderr, "Image size found: %d x %d\n", ImageWidth, ImageHeight );


	texture = new unsigned char[3 * nums * numt];
	if (texture == NULL)
	{
		fprintf(stderr, "Cannot allocate the texture array!\b");
		return NULL;
	}


	// extra padding bytes:

	numextra = 4 * (((3 * InfoHeader.biWidth) + 3) / 4) - 3 * InfoHeader.biWidth;


	// we do not support compression:

	if (InfoHeader.biCompression != birgb)
	{
		fprintf(stderr, "Wrong type of image compression: %d\n", InfoHeader.biCompression);
		fclose(fp);
		return NULL;
	}



	rewind(fp);
	fseek(fp, 14 + 40, SEEK_SET);

	if (InfoHeader.biBitCount == 24)
	{
		for (t = 0, tp = texture; t < numt; t++)
		{
			for (s = 0; s < nums; s++, tp += 3)
			{
				*(tp + 2) = fgetc(fp);		// b
				*(tp + 1) = fgetc(fp);		// g
				*(tp + 0) = fgetc(fp);		// r
			}

			for (e = 0; e < numextra; e++)
			{
				fgetc(fp);
			}
		}
	}

	fclose(fp);

	*width = nums;
	*height = numt;
	return texture;
}



int
ReadInt(FILE *fp)
{
	unsigned char b3, b2, b1, b0;
	b0 = fgetc(fp);
	b1 = fgetc(fp);
	b2 = fgetc(fp);
	b3 = fgetc(fp);
	return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}


short
ReadShort(FILE *fp)
{
	unsigned char b1, b0;
	b0 = fgetc(fp);
	b1 = fgetc(fp);
	return (b1 << 8) | b0;
}
//--------------------------

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef WIN32
#include <windows.h>
#pragma warning(disable:4996)
#include "glew.h"
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include "glut.h"
#include "glslprogram.h"
#define NVIDIA_SHADER_BINARY	0x00008e21		// nvidia binary enum
GLSLProgram *Pattern;


//extra
struct GLshadertype
{
	char *extension;
	GLenum name;
}
ShaderTypes[] =
{
	{ ".cs",   GL_COMPUTE_SHADER },
	{ ".vert", GL_VERTEX_SHADER },
	{ ".vs",   GL_VERTEX_SHADER },
	{ ".frag", GL_FRAGMENT_SHADER },
	{ ".fs",   GL_FRAGMENT_SHADER },
	{ ".geom", GL_GEOMETRY_SHADER },
	{ ".gs",   GL_GEOMETRY_SHADER },
	{ ".tcs",  GL_TESS_CONTROL_SHADER },
	{ ".tes",  GL_TESS_EVALUATION_SHADER },
};

struct GLbinarytype
{
	char *extension;
	GLenum format;
}
BinaryTypes[] =
{
	{ ".nvb",    NVIDIA_SHADER_BINARY },
};

extern char *Gstap;		// set later

static
char *
GetExtension(char *file)
{
	int n = (int)strlen(file) - 1;	// index of last non-null character

									// look for a '.':

	do
	{
		if (file[n] == '.')
			return &file[n];	// the extension includes the '.'
		n--;
	} while (n >= 0);

	// never found a '.':

	return NULL;
}


GLSLProgram::GLSLProgram()
{
	Verbose = false;
	InputTopology = GL_TRIANGLES;
	OutputTopology = GL_TRIANGLE_STRIP;

	CanDoComputeShaders = IsExtensionSupported("GL_ARB_compute_shader");
	CanDoVertexShaders = IsExtensionSupported("GL_ARB_vertex_shader");
	CanDoTessControlShaders = IsExtensionSupported("GL_ARB_tessellation_shader");
	CanDoTessEvaluationShaders = CanDoTessControlShaders;
	CanDoGeometryShaders = IsExtensionSupported("GL_EXT_geometry_shader4");
	CanDoFragmentShaders = IsExtensionSupported("GL_ARB_fragment_shader");
	CanDoBinaryFiles = IsExtensionSupported("GL_ARB_get_program_binary");

	fprintf(stderr, "Can do: ");
	if (CanDoComputeShaders)		fprintf(stderr, "compute shaders, ");
	if (CanDoVertexShaders)		fprintf(stderr, "vertex shaders, ");
	if (CanDoTessControlShaders)		fprintf(stderr, "tess control shaders, ");
	if (CanDoTessEvaluationShaders)	fprintf(stderr, "tess evaluation shaders, ");
	if (CanDoGeometryShaders)		fprintf(stderr, "geometry shaders, ");
	if (CanDoFragmentShaders)		fprintf(stderr, "fragment shaders, ");
	if (CanDoBinaryFiles)			fprintf(stderr, "binary shader files ");
	fprintf(stderr, "\n");
}


// this is what is exposed to the user
// file1 - file5 are defaulted as NULL if not given
// CreateHelper is a varargs procedure, so must end in a NULL argument,
//	which I know to supply but I'm worried users won't

bool
GLSLProgram::Create(char *file0, char *file1, char *file2, char *file3, char * file4, char *file5)
{
	return CreateHelper(file0, file1, file2, file3, file4, file5, NULL);
}


// this is the varargs version of the Create method

bool
GLSLProgram::CreateHelper(char *file0, ...)
{
	GLsizei n = 0;
	GLchar *buf;
	Valid = true;

	IncludeGstap = false;
	Cshader = Vshader = TCshader = TEshader = Gshader = Fshader = 0;
	Program = 0;
	AttributeLocs.clear();
	UniformLocs.clear();

	if (Program == 0)
	{
		Program = glCreateProgram();
		CheckGlErrors("glCreateProgram");
	}

	va_list args;
	va_start(args, file0);

	// This is a little dicey
	// There is no way, using var args, to know how many arguments were passed
	// I am depending on the caller passing in a NULL as the final argument.
	// If they don't, bad things will happen.

	char *file = file0;
	int type;
	while (file != NULL)
	{
		int maxBinaryTypes = sizeof(BinaryTypes) / sizeof(struct GLbinarytype);
		type = -1;
		char *extension = GetExtension(file);
		// fprintf( stderr, "File = '%s', extension = '%s'\n", file, extension );

		for (int i = 0; i < maxBinaryTypes; i++)
		{
			if (strcmp(extension, BinaryTypes[i].extension) == 0)
			{
				// fprintf( stderr, "Legal extension = '%s'\n", extension );
				LoadProgramBinary(file, BinaryTypes[i].format);
				break;
			}
		}

		int maxShaderTypes = sizeof(ShaderTypes) / sizeof(struct GLshadertype);
		for (int i = 0; i < maxShaderTypes; i++)
		{
			if (strcmp(extension, ShaderTypes[i].extension) == 0)
			{
				// fprintf( stderr, "Legal extension = '%s'\n", extension );
				type = i;
				break;
			}
		}

		if (type < 0)
		{
			fprintf(stderr, "Unknown filename extension: '%s'\n", extension);
			fprintf(stderr, "Legal Extensions are: ");
			for (int i = 0; i < maxBinaryTypes; i++)
			{
				if (i != 0)	fprintf(stderr, " , ");
				fprintf(stderr, "%s", BinaryTypes[i].extension);
			}
			fprintf(stderr, "\n");
			for (int i = 0; i < maxShaderTypes; i++)
			{
				if (i != 0)	fprintf(stderr, " , ");
				fprintf(stderr, "%s", ShaderTypes[i].extension);
			}
			fprintf(stderr, "\n");
			Valid = false;
			goto cont;
		}

		GLuint shader;
		switch (ShaderTypes[type].name)
		{
		case GL_COMPUTE_SHADER:
			if (!CanDoComputeShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle compute shaders\n");
				Valid = false;
				goto cont;
			}
			shader = glCreateShader(GL_COMPUTE_SHADER);
			break;

		case GL_VERTEX_SHADER:
			if (!CanDoVertexShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle vertex shaders\n");
				Valid = false;
				goto cont;
			}
			shader = glCreateShader(GL_VERTEX_SHADER);
			break;

		case GL_TESS_CONTROL_SHADER:
			if (!CanDoTessControlShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle tessellation control shaders\n");
				Valid = false;
				goto cont;
			}
			shader = glCreateShader(GL_TESS_CONTROL_SHADER);
			break;

		case GL_TESS_EVALUATION_SHADER:
			if (!CanDoTessEvaluationShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle tessellation evaluation shaders\n");
				Valid = false;
				goto cont;
			}
			shader = glCreateShader(GL_TESS_EVALUATION_SHADER);
			break;

		case GL_GEOMETRY_SHADER:
			if (!CanDoGeometryShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle geometry shaders\n");
				Valid = false;
				goto cont;
			}
			//glProgramParameteriEXT( Program, GL_GEOMETRY_INPUT_TYPE_EXT,  InputTopology );
			//glProgramParameteriEXT( Program, GL_GEOMETRY_OUTPUT_TYPE_EXT, OutputTopology );
			//glProgramParameteriEXT( Program, GL_GEOMETRY_VERTICES_OUT_EXT, 1024 );
			shader = glCreateShader(GL_GEOMETRY_SHADER);
			break;

		case GL_FRAGMENT_SHADER:
			if (!CanDoFragmentShaders)
			{
				fprintf(stderr, "Warning: this system cannot handle fragment shaders\n");
				Valid = false;
				goto cont;
			}
			shader = glCreateShader(GL_FRAGMENT_SHADER);
			break;
		}


		// read the shader source into a buffer:

		FILE * in;
		int length;
		FILE * logfile;

		in = fopen(file, "rb");
		if (in == NULL)
		{
			fprintf(stderr, "Cannot open shader file '%s'\n", file);
			Valid = false;
			goto cont;
		}

		fseek(in, 0, SEEK_END);
		length = ftell(in);
		fseek(in, 0, SEEK_SET);		// rewind

		buf = new GLchar[length + 1];
		fread(buf, sizeof(GLchar), length, in);
		buf[length] = '\0';
		fclose(in);

		GLchar *strings[2];
		int n = 0;

		if (IncludeGstap)
		{
			strings[n] = Gstap;
			n++;
		}

		strings[n] = buf;
		n++;

		// Tell GL about the source:

		glShaderSource(shader, n, (const GLchar **)strings, NULL);
		delete[] buf;
		CheckGlErrors("Shader Source");

		// compile:

		glCompileShader(shader);
		GLint infoLogLen;
		GLint compileStatus;
		CheckGlErrors("CompileShader:");
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

		if (compileStatus == 0)
		{
			fprintf(stderr, "Shader '%s' did not compile.\n", file);
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
			if (infoLogLen > 0)
			{
				GLchar *infoLog = new GLchar[infoLogLen + 1];
				glGetShaderInfoLog(shader, infoLogLen, NULL, infoLog);
				infoLog[infoLogLen] = '\0';
				logfile = fopen("glsllog.txt", "w");
				if (logfile != NULL)
				{
					fprintf(logfile, "\n%s\n", infoLog);
					fclose(logfile);
				}
				fprintf(stderr, "\n%s\n", infoLog);
				delete[] infoLog;
			}
			glDeleteShader(shader);
			Valid = false;
			goto cont;
		}
		else
		{
			if (Verbose)
				fprintf(stderr, "Shader '%s' compiled.\n", file);

			glAttachShader(this->Program, shader);
		}



	cont:
		// go to the next file:

		file = va_arg(args, char *);
	}

	va_end(args);

	// link the entire shader program:

	glLinkProgram(Program);
	CheckGlErrors("Link Shader 1");

	GLchar* infoLog;
	GLint infoLogLen;
	GLint linkStatus;
	glGetProgramiv(this->Program, GL_LINK_STATUS, &linkStatus);
	CheckGlErrors("Link Shader 2");

	if (linkStatus == 0)
	{
		glGetProgramiv(this->Program, GL_INFO_LOG_LENGTH, &infoLogLen);
		fprintf(stderr, "Failed to link program -- Info Log Length = %d\n", infoLogLen);
		if (infoLogLen > 0)
		{
			infoLog = new GLchar[infoLogLen + 1];
			glGetProgramInfoLog(this->Program, infoLogLen, NULL, infoLog);
			infoLog[infoLogLen] = '\0';
			fprintf(stderr, "Info Log:\n%s\n", infoLog);
			delete[] infoLog;

		}
		glDeleteProgram(Program);
		Valid = false;
	}
	else
	{
		if (Verbose)
			fprintf(stderr, "Shader Program linked.\n");
		// validate the program:

		GLint status;
		glValidateProgram(Program);
		glGetProgramiv(Program, GL_VALIDATE_STATUS, &status);
		if (status == GL_FALSE)
		{
			fprintf(stderr, "Program is invalid.\n");
			Valid = false;
		}
		else
		{
			if (Verbose)
				fprintf(stderr, "Shader Program validated.\n");
		}
	}

	return Valid;
}


void
GLSLProgram::DispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
	Use();
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}


bool
GLSLProgram::IsValid()
{
	return Valid;
}


bool
GLSLProgram::IsNotValid()
{
	return !Valid;
}


void
GLSLProgram::SetVerbose(bool v)
{
	Verbose = v;
}


void
GLSLProgram::Use()
{
	Use(this->Program);
};


void
GLSLProgram::Use(GLuint p)
{
	if (p != CurrentProgram)
	{
		glUseProgram(p);
		CurrentProgram = p;
	}
};


void
GLSLProgram::UseFixedFunction()
{
	this->Use(0);
};


int
GLSLProgram::GetAttributeLocation(char *name)
{
	std::map<char *, int>::iterator pos;

	pos = AttributeLocs.find(name);
	if (pos == AttributeLocs.end())
	{
		AttributeLocs[name] = glGetAttribLocation(this->Program, name);
	}

	return AttributeLocs[name];
};


#ifdef NOT_SUPPORTED
void
GLSLProgram::SetAttributeVariable(char* name, int val)
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		this->Use();
		glVertexAttrib1i(loc, val);
	}
};
#endif


void
GLSLProgram::SetAttributeVariable(char* name, float val)
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		this->Use();
		glVertexAttrib1f(loc, val);
	}
};


void
GLSLProgram::SetAttributeVariable(char* name, float val0, float val1, float val2)
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		this->Use();
		glVertexAttrib3f(loc, val0, val1, val2);
	}
};


void
GLSLProgram::SetAttributeVariable(char* name, float vals[3])
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		this->Use();
		glVertexAttrib3fv(loc, vals);
	}
};


#ifdef VEC3_H
void
GLSLProgram::SetAttributeVariable(char* name, Vec3& v);
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		float vec[3];
		v.GetVec3(vec);
		this->Use();
		glVertexAttrib3fv(loc, 3, vec);
	}
};
#endif


#ifdef VERTEX_BUFFER_OBJECT_H
void
GLSLProgram::SetAttributeVariable(char *name, VertexBufferObject& vb, GLenum which)
{
	int loc;
	if ((loc = GetAttributeLocation(name)) >= 0)
	{
		this->Use();
		glEnableVertexAttribArray(loc);
		switch (which)
		{
		case GL_VERTEX:
			glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(? ));
			break;

		case GL_NORMAL:
			glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(? ));
			break;

		case GL_COLOR:
			glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(? ));
			break;
		}
	};
#endif




	int
		GLSLProgram::GetUniformLocation(char *name)
	{
		std::map<char *, int>::iterator pos;

		pos = UniformLocs.find(name);
		//if( Verbose )
		//fprintf( stderr, "Uniform: pos = 0x%016x ; size = %d ; end = 0x%016x\n", pos, UniformLocs.size(), UniformLocs.end() );
		if (pos == UniformLocs.end())
		{
			GLuint loc = glGetUniformLocation(this->Program, name);
			UniformLocs[name] = loc;
			if (Verbose)
				fprintf(stderr, "Location of '%s' in Program %d = %d\n", name, this->Program, loc);
		}
		else
		{
			if (Verbose)
			{
				fprintf(stderr, "Location = %d\n", UniformLocs[name]);
				if (UniformLocs[name] == -1)
					fprintf(stderr, "Location of uniform variable '%s' is -1\n", name);
			}
		}

		return UniformLocs[name];
	};


	void
		GLSLProgram::SetUniformVariable(char* name, int val)
	{
		int loc;
		if ((loc = GetUniformLocation(name)) >= 0)
		{
			this->Use();
			glUniform1i(loc, val);
		}
	};


	void
		GLSLProgram::SetUniformVariable(char* name, float val)
	{
		int loc;
		if ((loc = GetUniformLocation(name)) >= 0)
		{
			this->Use();
			glUniform1f(loc, val);
		}
	};


	void
		GLSLProgram::SetUniformVariable(char* name, float val0, float val1, float val2)
	{
		int loc;
		if ((loc = GetUniformLocation(name)) >= 0)
		{
			this->Use();
			glUniform3f(loc, val0, val1, val2);
		}
	};


	void
		GLSLProgram::SetUniformVariable(char* name, float vals[3])
	{
		int loc;
		fprintf(stderr, "Found a 3-element array\n");

		if ((loc = GetUniformLocation(name)) >= 0)
		{
			this->Use();
			glUniform3fv(loc, 3, vals);
		}
	};


#ifdef VEC3_H
	void
		GLSLProgram::SetUniformVariable(char* name, Vec3& v);
	{
		int loc;
		if ((loc = GetAttributeLocation(name)) >= 0)
		{
			float vec[3];
			v.GetVec3(vec);
			this->Use();
			glUniform3fv(loc, 3, vec);
		}
	};
#endif


#ifdef MATRIX4_H
	void
		GLSLProgram::SetUniformVariable(char* name, Matrix4& m)
	{
		int loc;
		if ((loc = GetUniformLocation(name)) >= 0)
		{
			float mat[4][4];
			m.GetMatrix4(mat);
			this->Use();
			glUniformMatrix4fv(loc, 16, true, mat);
		}
	};
#endif


	void
		GLSLProgram::SetInputTopology(GLenum t)
	{
		if (t != GL_POINTS  && t != GL_LINES  &&  t != GL_LINES_ADJACENCY_EXT  &&  t != GL_TRIANGLES  &&  t != GL_TRIANGLES_ADJACENCY_EXT)
		{
			fprintf(stderr, "Warning: You have not specified a supported Input Topology\n");
		}
		InputTopology = t;
	}


	void
		GLSLProgram::SetOutputTopology(GLenum t)
	{
		if (t != GL_POINTS  && t != GL_LINE_STRIP  &&  t != GL_TRIANGLE_STRIP)
		{
			fprintf(stderr, "Warning: You have not specified a supported Onput Topology\n");
		}
		OutputTopology = t;
	}




	bool
		GLSLProgram::IsExtensionSupported(const char *extension)
	{
		// see if the extension is bogus:

		if (extension == NULL || extension[0] == '\0')
			return false;

		GLubyte *where = (GLubyte *)strchr(extension, ' ');
		if (where != 0)
			return false;

		// get the full list of extensions:

		const GLubyte *extensions = glGetString(GL_EXTENSIONS);

		for (const GLubyte *start = extensions; ; )
		{
			where = (GLubyte *)strstr((const char *)start, extension);
			if (where == 0)
				return false;

			GLubyte *terminator = where + strlen(extension);

			if (where == start || *(where - 1) == '\n' || *(where - 1) == ' ')
				if (*terminator == ' ' || *terminator == '\n' || *terminator == '\0')
					return true;
			start = terminator;
		}
		return false;
	}


	int GLSLProgram::CurrentProgram = 0;




#ifndef CHECK_GL_ERRORS
#define CHECK_GL_ERRORS
	void
		CheckGlErrors(const char* caller)
	{
		unsigned int gle = glGetError();

		if (gle != GL_NO_ERROR)
		{
			fprintf(stderr, "GL Error discovered from caller %s: ", caller);
			switch (gle)
			{
			case GL_INVALID_ENUM:
				fprintf(stderr, "Invalid enum.\n");
				break;
			case GL_INVALID_VALUE:
				fprintf(stderr, "Invalid value.\n");
				break;
			case GL_INVALID_OPERATION:
				fprintf(stderr, "Invalid Operation.\n");
				break;
			case GL_STACK_OVERFLOW:
				fprintf(stderr, "Stack overflow.\n");
				break;
			case GL_STACK_UNDERFLOW:
				fprintf(stderr, "Stack underflow.\n");
				break;
			case GL_OUT_OF_MEMORY:
				fprintf(stderr, "Out of memory.\n");
				break;
			}
			return;
		}
	}
#endif



	void
		GLSLProgram::SaveProgramBinary(const char * fileName, GLenum * format)
	{
		glProgramParameteri(this->Program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
		GLint length;
		glGetProgramiv(this->Program, GL_PROGRAM_BINARY_LENGTH, &length);
		GLubyte *buffer = new GLubyte[length];
		glGetProgramBinary(this->Program, length, NULL, format, buffer);

		fprintf(stderr, "Program binary format = 0x%04x\n", *format);

		FILE * fpout = fopen(fileName, "wb");
		if (fpout == NULL)
		{
			fprintf(stderr, "Cannot create output GLSL binary file '%s'\n", fileName);
			return;
		}
		fwrite(buffer, length, 1, fpout);
		fclose(fpout);
		delete[] buffer;
	}


	void
		GLSLProgram::LoadProgramBinary(const char * fileName, GLenum format)
	{
		FILE *fpin = fopen(fileName, "rb");
		if (fpin == NULL)
		{
			fprintf(stderr, "Cannot open input GLSL binary file '%s'\n", fileName);
			return;
		}
		fseek(fpin, 0, SEEK_END);
		GLint length = (GLint)ftell(fpin);
		GLubyte *buffer = new GLubyte[length];
		rewind(fpin);
		fread(buffer, length, 1, fpin);
		fclose(fpin);

		glProgramBinary(this->Program, format, buffer, length);
		delete[] buffer;

		GLint   success;
		glGetProgramiv(this->Program, GL_LINK_STATUS, &success);

		if (!success)
		{
			fprintf(stderr, "Did not successfully load the GLSL binary file '%s'\n", fileName);
			return;
		}
	}



	void
		GLSLProgram::SetGstap(bool b)
	{
		IncludeGstap = b;
	}


	GLchar *Gstap =
	{
		"#ifndef GSTAP_H\n\
#define GSTAP_H\n\
\n\
\n\
// gstap.h -- useful for glsl migration\n\
// from:\n\
//		Mike Bailey and Steve Cunningham\n\
//		\"Graphics Shaders: Theory and Practice\",\n\
//		Second Edition, AK Peters, 2011.\n\
\n\
\n\
\n\
// we are assuming that the compatibility #version line\n\
// is given in the source file, for example:\n\
// #version 400 compatibility\n\
\n\
\n\
// uniform variables:\n\
\n\
#define uModelViewMatrix		gl_ModelViewMatrix\n\
#define uProjectionMatrix		gl_ProjectionMatrix\n\
#define uModelViewProjectionMatrix	gl_ModelViewProjectionMatrix\n\
#define uNormalMatrix			gl_NormalMatrix\n\
#define uModelViewMatrixInverse		gl_ModelViewMatrixInverse\n\
\n\
// attribute variables:\n\
\n\
#define aColor				gl_Color\n\
#define aNormal				gl_Normal\n\
#define aVertex				gl_Vertex\n\
\n\
#define aTexCoord0			gl_MultiTexCoord0\n\
#define aTexCoord1			gl_MultiTexCoord1\n\
#define aTexCoord2			gl_MultiTexCoord2\n\
#define aTexCoord3			gl_MultiTexCoord3\n\
#define aTexCoord4			gl_MultiTexCoord4\n\
#define aTexCoord5			gl_MultiTexCoord5\n\
#define aTexCoord6			gl_MultiTexCoord6\n\
#define aTexCoord7			gl_MultiTexCoord7\n\
\n\
\n\
#endif		// #ifndef GSTAP_H\n\
\n\
\n"
	};
//end extra

//	This is a sample OpenGL / GLUT program
//
//	The objective is to draw a 3d object and change the color of the axes
//		with a glut menu
//
//	The left mouse button does rotation
//	The middle mouse button does scaling
//	The user interface allows:
//		1. The axes to be turned on and off
//		2. The color of the axes to be changed
//		3. Debugging to be turned on and off
//		4. Depth cueing to be turned on and off
//		5. The projection to be changed
//		6. The transformations to be reset
//		7. The program to quit
//
//	Author:			Joe Graphics

// NOTE: There are a lot of good reasons to use const variables instead
// of #define's.  However, Visual C++ does not allow a const variable
// to be used as an array size or as the case in a switch( ) statement.  So in
// the following, all constants are const variables except those which need to
// be array sizes or cases in switch( ) statements.  Those are #defines.



// title of these windows:

const char *WINDOWTITLE = { "Project #1 -- Alex Davis" };
const char *GLUITITLE   = { "User Interface Window" };


// what the glui package defines as true and false:

const int GLUITRUE  = { true  };
const int GLUIFALSE = { false };


// the escape key:

#define ESCAPE		0x1b


// initial window size:

const int INIT_WINDOW_SIZE = { 600 };


// size of the box:

const float BOXSIZE = { 2.f };



// multiplication factors for input interaction:
//  (these are known from previous experience)

const float ANGFACT = { 1. };
const float SCLFACT = { 0.005f };


// minimum allowable scale factor:

const float MINSCALE = { 0.05f };


// active mouse buttons (or them together):

const int LEFT   = { 4 };
const int MIDDLE = { 2 };
const int RIGHT  = { 1 };


// which projection:

enum Projections
{
	ORTHO,
	PERSP
};


// which button:

enum ButtonVals
{
	RESET,
	QUIT
};


// window background color (rgba):

const GLfloat BACKCOLOR[ ] = { 0., 0., 0., 1. };


// line width for the axes:

const GLfloat AXES_WIDTH   = { 3. };


// the color numbers:
// this order must match the radio button order

enum Colors
{
	RED,
	YELLOW,
	GREEN,
	CYAN,
	BLUE,
	MAGENTA,
	WHITE,
	BLACK
};

char * ColorNames[ ] =
{
	"Red",
	"Yellow",
	"Green",
	"Cyan",
	"Blue",
	"Magenta",
	"White",
	"Black"
};


// the color definitions:
// this order must match the menu order

const GLfloat Colors[ ][3] = 
{
	{ 1., 0., 0. },		// red
	{ 1., 1., 0. },		// yellow
	{ 0., 1., 0. },		// green
	{ 0., 1., 1. },		// cyan
	{ 0., 0., 1. },		// blue
	{ 1., 0., 1. },		// magenta
	{ 1., 1., 1. },		// white
	{ 0., 0., 0. },		// black
};


// fog parameters:

const GLfloat FOGCOLOR[4] = { .0, .0, .0, 1. };
const GLenum  FOGMODE     = { GL_LINEAR };
const GLfloat FOGDENSITY  = { 0.30f };
const GLfloat FOGSTART    = { 1.5 };
const GLfloat FOGEND      = { 4. };


// non-constant global variables:

int		ActiveButton;			// current button that is down
GLuint	AxesList;				// list to hold the axes
int		AxesOn;					// != 0 means to draw the axes
int		DebugOn;				// != 0 means to print debugging info
int		DepthCueOn;				// != 0 means to use intensity depth cueing
GLuint	BoxList;				// object display list
int		MainWindow;				// window id for main graphics window
float	Scale;					// scaling factor
int		WhichColor;				// index into Colors[ ]
int		WhichProjection;		// ORTHO or PERSP
int		WhichVeiw;
int		Xmouse, Ymouse;			// mouse values
float	Xrot, Yrot;				// rotation angles in degrees
int Light0On = 0;
int Light1On = 0;
int Freeze = 0;


// function prototypes:

void	Animate( );
void	Display( );
void	DoAxesMenu( int );
void	DoColorMenu( int );
void	DoDepthMenu( int );
void	DoDebugMenu( int );
void	DoMainMenu( int );
void	DoProjectMenu( int );
void	DoViewMenu(int);
void	DoRasterString( float, float, float, char * );
void	DoStrokeString( float, float, float, float, char * );
float	ElapsedSeconds( );
void	InitGraphics( );
void	InitLists( );
void	InitMenus( );
void	Keyboard( unsigned char, int, int );
void	MouseButton( int, int, int, int );
void	MouseMotion( int, int );
void	Reset( );
void	Resize( int, int );
void	Visibility( int );

void	Axes( float );
void	HsvRgb( float[3], float [3] );

unsigned char *Texture;
float tex;


// main program:

int
main( int argc, char *argv[ ] )
{
	// turn on the glut package:
	// (do this before checking argc and argv since it might
	// pull some command line arguments out)

	glutInit( &argc, argv );


	// setup all the graphics stuff:

	InitGraphics( );


	// create the display structures that will not change:

	InitLists( );


	// init all the global variables used by Display( ):
	// this will also post a redisplay

	Reset( );


	// setup all the user interface stuff:

	InitMenus( );

	Pattern = new GLSLProgram();
	bool valid = Pattern->Create("pattern.vert", "pattern.frag");
	if (!valid)
	{
		//. . .
	}

	//Bumpmap

	int width, height;

	width = 1024;
	height = 512;

	Texture = BmpToTexture("worldtex.bmp", &width, &height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, Texture);
	// draw the scene once and wait for some interaction:
	// (this will never return)

	glutSetWindow( MainWindow );
	glutMainLoop( );


	// this is here to make the compiler happy:

	return 0;
}


// this is where one would put code that is to be called
// everytime the glut main loop has nothing to do
//
// this is typically where animation parameters are set
//
// do not call Display( ) from here -- let glutMainLoop( ) do it

void
Animate( )
{
	// put animation stuff in here -- change some global variables
	// for Display( ) to find:
	tex += 0.0009;
	if (tex >= 2)
		tex = -2;
	//printf("working");
	// force a call to Display( ) next time it is convenient:

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}

//lighting project
float White[] = { 1.,1.,1.,1. };
// utility to create an array from 3 separate values:
float *
Array3(float a, float b, float c)
{
	static float array[4];
	array[0] = a;
	array[1] = b;
	array[2] = c;
	array[3] = 1.;
	return array;
}
// utility to create an array from a multiplier and an array:
float *
MulArray3(float factor, float array0[3])
{
	static float array[4];
	array[0] = factor * array0[0];
	array[1] = factor * array0[1];
	array[2] = factor * array0[2];
	array[3] = 1.;
	return array;
}

void
SetMaterial(float r, float g, float b, float shininess)
{
	glMaterialfv(GL_BACK, GL_EMISSION, Array3(0., 0., 0.));
	glMaterialfv(GL_BACK, GL_AMBIENT, MulArray3(.4f, White));
	glMaterialfv(GL_BACK, GL_DIFFUSE, MulArray3(.5, White));
	glMaterialfv(GL_BACK, GL_SPECULAR, Array3(0., 0., 0.));
	glMaterialf(GL_BACK, GL_SHININESS, 2.f);
	glMaterialfv(GL_FRONT, GL_EMISSION, Array3(0., 0., 0.));
	glMaterialfv(GL_FRONT, GL_AMBIENT, Array3(r, g, b));
	glMaterialfv(GL_FRONT, GL_DIFFUSE, Array3(r, g, b));
	glMaterialfv(GL_FRONT, GL_SPECULAR, MulArray3(.8f, White));
	glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}

void
SetPointLight(int ilight, float x, float y, float z, float r, float g, float b)
{
	glLightfv(ilight, GL_POSITION, Array3(x, y, z));
	glLightfv(ilight, GL_AMBIENT, Array3(0., 0., 0.));
	glLightfv(ilight, GL_DIFFUSE, Array3(r, g, b));
	glLightfv(ilight, GL_SPECULAR, Array3(r, g, b));
	glLightf(ilight, GL_CONSTANT_ATTENUATION, 1.);
	glLightf(ilight, GL_LINEAR_ATTENUATION, 0.);
	glLightf(ilight, GL_QUADRATIC_ATTENUATION, 0.);
	glEnable(ilight);
}
void
SetSpotLight(int ilight, float x, float y, float z, float xdir, float ydir, float zdir, float r, float g, float b)
{
	glLightfv(ilight, GL_POSITION, Array3(x, y, z));
	glLightfv(ilight, GL_SPOT_DIRECTION, Array3(xdir, ydir, zdir));
	glLightf(ilight, GL_SPOT_EXPONENT, 1.);
	glLightf(ilight, GL_SPOT_CUTOFF, 45.);
	glLightfv(ilight, GL_AMBIENT, Array3(0., 0., 0.));
	glLightfv(ilight, GL_DIFFUSE, Array3(r, g, b));
	glLightfv(ilight, GL_SPECULAR, Array3(r, g, b));
	glLightf(ilight, GL_CONSTANT_ATTENUATION, 1.);
	glLightf(ilight, GL_LINEAR_ATTENUATION, 0.);
	glLightf(ilight, GL_QUADRATIC_ATTENUATION, 0.);
	glEnable(ilight);
}


// draw the complete scene:

void
Display( )
{
	if( DebugOn != 0 )
	{
		fprintf( stderr, "Display\n" );
	}


	// set which window we want to do the graphics into:

	glutSetWindow( MainWindow );


	// erase the background:

	glDrawBuffer( GL_BACK );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );


	// specify shading to be flat:

	glShadeModel( GL_FLAT );


	// set the viewport to a square centered in the window:

	GLsizei vx = glutGet( GLUT_WINDOW_WIDTH );
	GLsizei vy = glutGet( GLUT_WINDOW_HEIGHT );
	GLsizei v = vx < vy ? vx : vy;			// minimum dimension
	GLint xl = ( vx - v ) / 2;
	GLint yb = ( vy - v ) / 2;
	glViewport( xl, yb,  v, v );


	// set the viewing volume:
	// remember that the Z clipping  values are actually
	// given as DISTANCES IN FRONT OF THE EYE
	// USE gluOrtho2D( ) IF YOU ARE DOING 2D !

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity( );
	if( WhichProjection == ORTHO )
		glOrtho( -3., 3.,     -3., 3.,     0.1, 1000. );
	else
		gluPerspective( 90., 1.,	0.1, 1000. );


	// place the objects into the scene:

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );


	// set the eye position, look-at position, and up-vector:

	gluLookAt( 0., 0., 3.,     0., 0., 0.,     0., 1., 0. );


	// rotate the scene:

	glRotatef( (GLfloat)Yrot, 0., 1., 0. );
	glRotatef( (GLfloat)Xrot, 1., 0., 0. );


	// uniformly scale the scene:

	if( Scale < MINSCALE )
		Scale = MINSCALE;
	glScalef( (GLfloat)Scale, (GLfloat)Scale, (GLfloat)Scale );


	// set the fog parameters:

	if( DepthCueOn != 0 )
	{
		glFogi( GL_FOG_MODE, FOGMODE );
		glFogfv( GL_FOG_COLOR, FOGCOLOR );
		glFogf( GL_FOG_DENSITY, FOGDENSITY );
		glFogf( GL_FOG_START, FOGSTART );
		glFogf( GL_FOG_END, FOGEND );
		glEnable( GL_FOG );
	}
	else
	{
		glDisable( GL_FOG );
	}


	// possibly draw the axes:

	/*if( AxesOn != 0 )
	{
		glColor3fv( &Colors[WhichColor][0] );
		glCallList( AxesList );
	}*/

	//patten stuff

	float X0, T0;
	float Ds, Dt;
	float V0, V1, V2;
	float color[3];
	Point p0, p1, p2, p3;

	glPointSize(3.);
	for (int i = 0; i < 10; i = i + 2) {
		if (i % 2 == 0)
			color[0] = 0.;
		else
			color[0] = 1.;
		if (i % 3 == 0)
			color[1] = 0.;
		else
			color[1] = 1.;
		if (i % 5 == 0)
			color[2] = 0.;
		else
			color[2] = 1.;

		p0.x = -1. * i;
		p0.y = 0;
		p0.z = i * tex;
		p1.x = -1. * i;
		p1.y = 1. * (i + 1);
		p1.z = i * tex;
		p2.x = 1. * (i + 1);
		p2.y = 1. * (i + 1);
		p2.z = (i + 1.) * tex;
		p3.x = 1. * (i + 1);
		p3.y = 0.;
		p3.z = (i + 1.) * tex;

		//test
		glLineWidth(3.);
		glColor3f(1., color[1], color[2]);
		glBegin(GL_LINE_STRIP);
		for (int it = 0; it <= 100; it++)
		{
			float t = (float)it / (float)100;
			float omt = 1.f - t;
			float x = omt*omt*omt*p0.x + 3.f*t*omt*omt*p1.x + 3.f*t*t*omt*p2.x + t*t*t*p3.x;
			float y = omt*omt*omt*p0.y + 3.f*t*omt*omt*p1.y + 3.f*t*t*omt*p2.y + t*t*t*p3.y;
			float z = omt*omt*omt*p0.z + 3.f*t*omt*omt*p1.z + 3.f*t*t*omt*p2.z + t*t*t*p3.z;
			glVertex3f(x, y, z);
		}
		glEnd();
		if ((WhichVeiw == 1) || (WhichVeiw == 0)) {
			glBegin(GL_POINTS);
			glVertex3f(p0.x, p0.y, p0.z);
			glVertex3f(p1.x, p1.y, p1.z);
			glVertex3f(p2.x, p2.y, p2.z);
			glVertex3f(p3.x, p3.y, p3.z);
			glEnd();
		}
		glLineWidth(1.); 
		if ((WhichVeiw == 0) || (WhichVeiw == 2)) {
			glBegin(GL_LINE_STRIP);
			glVertex3f(p0.x, p0.y, p0.z);
			glVertex3f(p1.x, p1.y, p1.z);
			glVertex3f(p2.x, p2.y, p2.z);
			glVertex3f(p3.x, p3.y, p3.z);
			glEnd();
		}


		p3.x = -1. * (i + 2);
		p3.y = 0;
		p3.z = (i + 2.) * tex;
		p2.x = -1. * (i + 2);
		p2.y = -1. * (i + 2);
		p2.z = (i + 2.) * tex;
		p1.x = 1. * (i + 1);
		p1.y = -1. * (i + 2);
		p1.z = (i + 1.) * tex;
		p0.x = 1. * (i + 1);
		p0.y = 0.;
		p0.z = (i + 1.) * tex;

		//test
		glLineWidth(3.);
		glColor3f(1., color[1], color[2]);
		glBegin(GL_LINE_STRIP);
		for (int it = 0; it <= 100; it++)
		{
			float t = (float)it / (float)100;
			float omt = 1.f - t;
			float x = omt*omt*omt*p0.x + 3.f*t*omt*omt*p1.x + 3.f*t*t*omt*p2.x + t*t*t*p3.x;
			float y = omt*omt*omt*p0.y + 3.f*t*omt*omt*p1.y + 3.f*t*t*omt*p2.y + t*t*t*p3.y;
			float z = omt*omt*omt*p0.z + 3.f*t*omt*omt*p1.z + 3.f*t*t*omt*p2.z + t*t*t*p3.z;
			glVertex3f(x, y, z);
		}
		glEnd();
		if ((WhichVeiw == 1) || (WhichVeiw == 0)) {
			glBegin(GL_POINTS);
			glVertex3f(p0.x, p0.y, p0.z);
			glVertex3f(p1.x, p1.y, p1.z);
			glVertex3f(p2.x, p2.y, p2.z);
			glVertex3f(p3.x, p3.y, p3.z);
			glEnd();
		}
		glLineWidth(1.);
		if ((WhichVeiw == 0) || (WhichVeiw == 2)) {
			glBegin(GL_LINE_STRIP);
			glVertex3f(p0.x, p0.y, p0.z);
			glVertex3f(p1.x, p1.y, p1.z);
			glVertex3f(p2.x, p2.y, p2.z);
			glVertex3f(p3.x, p3.y, p3.z);
			glEnd();
		}

	}
	/*float color[3];
	int i = 6;
	float dz = BOXSIZE / 10.f;

	// since we are using glScalef( ), be sure normals get unitized:

	glEnable( GL_NORMALIZE );

	//lighting
	if (i % 2 == 0)
		color[0] = 0.;
	else
		color[0] = 1.;
	if (i % 3 == 0)
		color[1] = 0.;
	else
		color[1] = 1.;
	if (i % 5 == 0)
		color[2] = 0.;
	else
		color[2] = 1.;
	glColor3f(color[0], color[1], color[2]);
	SetSpotLight(GL_LIGHT0, 5. * cos(tex), 5. * sin(tex), 5. * sin(tex), -1., 0., 0., color[0], color[1], color[2]);
	glPushMatrix();
	glTranslatef(5. * cos(tex), 5. * sin(tex), 5. * sin(tex));
	SetMaterial(color[0], color[1], color[2], .1);
	glutSolidSphere(.1, 20, 10);
	glPopMatrix();


	glColor3f(1., 1., 1.);
	SetPointLight(GL_LIGHT1, -4., 0., 0., 1., 1., 1.);
	glPushMatrix();
	glColor3f(1., 1., 1.);
	glTranslatef(-4., 0., 0.);
	SetMaterial(1., 1., 1., .1);
	glutSolidSphere(.1, 20, 10);
	glPopMatrix();

	glEnable(GL_LIGHTING);
	if (Light0On == 0)
		glEnable(GL_LIGHT0);
	else
		glDisable(GL_LIGHT0);
	if (Light1On == 0)
		glEnable(GL_LIGHT1);
	else
		glDisable(GL_LIGHT1);


	//objects
	glPushMatrix();
	i = 3;
	if (i % 2 == 0)
		color[0] = 0.;
	else
		color[0] = 1.;
	if (i % 3 == 0)
		color[1] = 0.;
	else
		color[1] = 1.;
	if (i % 5 == 0)
		color[2] = 0.;
	else
		color[2] = 1.;
	glColor3f(color[0], color[1], color[2]);

	glShadeModel(GL_FLAT);
	SetMaterial(color[0], color[1], color[2], .1);
	glutSolidSphere(1, 20, 10);*/
	/*if(WhichVeiw != 0)
		glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);

	for (i = 0;i < 100; i++) {
		if (i % 2 == 0)
			color[0] = 0.;
		else
			color[0] = 1.;
		if (i % 3 == 0)
			color[1] = 0.;
		else
			color[1] = 1.;
		if (i % 5 == 0)
			color[2] = 0.;
		else
			color[2] = 1.;

		glColor3f(color[0], color[1], color[2]);

		if (WhichVeiw == 1 || WhichVeiw == 0) {
			glTexCoord2f(0.4 - i*0.004, 0.4 - i*0.004);
			glVertex3f(-(i / 5.f), -(i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.6 + i*0.004, 0.4 - i*0.004);
			glVertex3f((i / 5.f), -(i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.6 + i*0.004, 0.6 + i*0.004);
			glVertex3f((i / 5.f), (i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.4 - i*0.004, 0.6 + i*0.004);
			glVertex3f(-(i / 5.f), (i / 5.f), -dz - 0.1 * i);
		}
		if (WhichVeiw == 2) {
			glTexCoord2f(0.4 - i*0.004 + tex, 0.4 - i*0.004);
			glVertex3f(-(i / 5.f), -(i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.6 + i*0.004 + tex, 0.4 - i*0.004);
			glVertex3f((i / 5.f), -(i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.6 + i*0.004 + tex, 0.6 + i*0.004);
			glVertex3f((i / 5.f), (i / 5.f), -dz - 0.1 * i);
			glTexCoord2f(0.4 - i*0.004 + tex, 0.6 + i*0.004);
			glVertex3f(-(i / 5.f), (i / 5.f), -dz - 0.1 * i);
		}
	}

	if (WhichVeiw != 0)
		glDisable(GL_TEXTURE_2D);
	glEnd();*/
	/*glPopMatrix();
	glPushMatrix();
	i = 11;
	if (i % 2 == 0)
		color[0] = 0.;
	else
		color[0] = 1.;
	if (i % 3 == 0)
		color[1] = 0.;
	else
		color[1] = 1.;
	if (i % 5 == 0)
		color[2] = 0.;
	else
		color[2] = 1.;

	glTranslatef(5. * cos(tex) + 0.6, 5. * sin(tex), 5. * sin(tex));
	glColor3f(color[0], color[1], color[2]);

	glShadeModel(GL_SMOOTH);
	SetMaterial(color[0], color[1], color[2], .1);
	glutSolidCube(1);
	glPopMatrix();
	glPushMatrix();
	i = 15;
	if (i % 2 == 0)
		color[0] = 0.;
	else
		color[0] = 1.;
	if (i % 3 == 0)
		color[1] = 0.;
	else
		color[1] = 1.;
	if (i % 5 == 0)
		color[2] = 0.;
	else
		color[2] = 1.;

	glTranslatef(0., 4.5, 4.5);
	glRotatef(90., 0., 1., 0.);
	glColor3f(color[0], color[1], color[2]);

	glShadeModel(GL_SMOOTH);
	SetMaterial(color[0], color[1], color[2], 10.);
	glutSolidTorus(1., 2.5, 50, 50);
	glPopMatrix();*/

	// draw the current object:

	//glCallList( BoxList );


	// draw some gratuitous text that just rotates on top of the scene:

	/*glDisable( GL_DEPTH_TEST );
	glColor3f( 0., 1., 1. );
	DoRasterString( 0., 1., 0., "Text That Moves" );*/


	// draw some gratuitous text that is fixed on the screen:
	//
	// the projection matrix is reset to define a scene whose
	// world coordinate system goes from 0-100 in each axis
	//
	// this is called "percent units", and is just a convenience
	//
	// the modelview matrix is reset to identity as we don't
	// want to transform these coordinates

	/*glDisable( GL_DEPTH_TEST );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity( );
	gluOrtho2D( 0., 100.,     0., 100. );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glColor3f( 1., 1., 1. );
	DoRasterString( 5., 5., 0., "Text That Doesn't" );*/


	// swap the double-buffered framebuffers:

	glutSwapBuffers( );


	// be sure the graphics buffer has been sent:
	// note: be sure to use glFlush( ) here, not glFinish( ) !

	glFlush( );
}


void
DoAxesMenu( int id )
{
	AxesOn = id;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


void
DoColorMenu( int id )
{
	WhichColor = id - RED;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


void
DoDebugMenu( int id )
{
	DebugOn = id;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


void
DoDepthMenu( int id )
{
	DepthCueOn = id;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


// main menu callback:

void
DoMainMenu( int id )
{
	switch( id )
	{
		case RESET:
			Reset( );
			break;

		case QUIT:
			// gracefully close out the graphics:
			// gracefully close the graphics window:
			// gracefully exit the program:
			glutSetWindow( MainWindow );
			glFinish( );
			glutDestroyWindow( MainWindow );
			exit( 0 );
			break;

		default:
			fprintf( stderr, "Don't know what to do with Main Menu ID %d\n", id );
	}

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


void
DoProjectMenu( int id )
{
	WhichProjection = id;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}

void
DoVeiwMenu(int id)
{
	WhichVeiw = id;

	glutSetWindow(MainWindow);
	glutPostRedisplay();
}

// use glut to display a string of characters using a raster font:

void
DoRasterString( float x, float y, float z, char *s )
{
	glRasterPos3f( (GLfloat)x, (GLfloat)y, (GLfloat)z );

	char c;			// one character to print
	for( ; ( c = *s ) != '\0'; s++ )
	{
		glutBitmapCharacter( GLUT_BITMAP_TIMES_ROMAN_24, c );
	}
}


// use glut to display a string of characters using a stroke font:

void
DoStrokeString( float x, float y, float z, float ht, char *s )
{
	glPushMatrix( );
		glTranslatef( (GLfloat)x, (GLfloat)y, (GLfloat)z );
		float sf = ht / ( 119.05f + 33.33f );
		glScalef( (GLfloat)sf, (GLfloat)sf, (GLfloat)sf );
		char c;			// one character to print
		for( ; ( c = *s ) != '\0'; s++ )
		{
			glutStrokeCharacter( GLUT_STROKE_ROMAN, c );
		}
	glPopMatrix( );
}


// return the number of seconds since the start of the program:

float
ElapsedSeconds( )
{
	// get # of milliseconds since the start of the program:

	int ms = glutGet( GLUT_ELAPSED_TIME );

	// convert it to seconds:

	return (float)ms / 1000.f;
}


// initialize the glui window:

void
InitMenus( )
{
	glutSetWindow( MainWindow );

	int numColors = sizeof( Colors ) / ( 3*sizeof(int) );
	int colormenu = glutCreateMenu( DoColorMenu );
	for( int i = 0; i < numColors; i++ )
	{
		glutAddMenuEntry( ColorNames[i], i );
	}

	int axesmenu = glutCreateMenu( DoAxesMenu );
	glutAddMenuEntry( "Off",  0 );
	glutAddMenuEntry( "On",   1 );

	int depthcuemenu = glutCreateMenu( DoDepthMenu );
	glutAddMenuEntry( "Off",  0 );
	glutAddMenuEntry( "On",   1 );

	int debugmenu = glutCreateMenu( DoDebugMenu );
	glutAddMenuEntry( "Off",  0 );
	glutAddMenuEntry( "On",   1 );

	int projmenu = glutCreateMenu( DoProjectMenu );
	glutAddMenuEntry( "Orthographic",  ORTHO );
	glutAddMenuEntry( "Perspective",   PERSP );

	int veiwmenu = glutCreateMenu(DoVeiwMenu);
	glutAddMenuEntry("Both", 0);
	glutAddMenuEntry("Points", 1);
	glutAddMenuEntry("Lines", 2);
	glutAddMenuEntry("None", 3);

	int mainmenu = glutCreateMenu( DoMainMenu );
	glutAddSubMenu(   "Axes",          axesmenu);
	glutAddSubMenu(   "Colors",        colormenu);
	glutAddSubMenu(   "Depth Cue",     depthcuemenu);
	glutAddSubMenu(   "Control",    veiwmenu );
	glutAddSubMenu("Projection", projmenu);
	glutAddMenuEntry( "Reset",         RESET );
	glutAddSubMenu(   "Debug",         debugmenu);
	glutAddMenuEntry( "Quit",          QUIT );

// attach the pop-up menu to the right mouse button:

	glutAttachMenu( GLUT_RIGHT_BUTTON );
}



// initialize the glut and OpenGL libraries:
//	also setup display lists and callback functions

void
InitGraphics( )
{
	// request the display modes:
	// ask for red-green-blue-alpha color, double-buffering, and z-buffering:

	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );

	// set the initial window configuration:

	glutInitWindowPosition( 0, 0 );
	glutInitWindowSize( INIT_WINDOW_SIZE, INIT_WINDOW_SIZE );

	// open the window and set its title:

	MainWindow = glutCreateWindow( WINDOWTITLE );
	glutSetWindowTitle( WINDOWTITLE );

	// set the framebuffer clear values:

	glClearColor( BACKCOLOR[0], BACKCOLOR[1], BACKCOLOR[2], BACKCOLOR[3] );

	// setup the callback functions:
	// DisplayFunc -- redraw the window
	// ReshapeFunc -- handle the user resizing the window
	// KeyboardFunc -- handle a keyboard input
	// MouseFunc -- handle the mouse button going down or up
	// MotionFunc -- handle the mouse moving with a button down
	// PassiveMotionFunc -- handle the mouse moving with a button up
	// VisibilityFunc -- handle a change in window visibility
	// EntryFunc	-- handle the cursor entering or leaving the window
	// SpecialFunc -- handle special keys on the keyboard
	// SpaceballMotionFunc -- handle spaceball translation
	// SpaceballRotateFunc -- handle spaceball rotation
	// SpaceballButtonFunc -- handle spaceball button hits
	// ButtonBoxFunc -- handle button box hits
	// DialsFunc -- handle dial rotations
	// TabletMotionFunc -- handle digitizing tablet motion
	// TabletButtonFunc -- handle digitizing tablet button hits
	// MenuStateFunc -- declare when a pop-up menu is in use
	// TimerFunc -- trigger something to happen a certain time from now
	// IdleFunc -- what to do when nothing else is going on

	glutSetWindow( MainWindow );
	glutDisplayFunc( Display );
	glutReshapeFunc( Resize );
	glutKeyboardFunc( Keyboard );
	glutMouseFunc( MouseButton );
	glutMotionFunc( MouseMotion );
	glutPassiveMotionFunc( NULL );
	glutVisibilityFunc( Visibility );
	glutEntryFunc( NULL );
	glutSpecialFunc( NULL );
	glutSpaceballMotionFunc( NULL );
	glutSpaceballRotateFunc( NULL );
	glutSpaceballButtonFunc( NULL );
	glutButtonBoxFunc( NULL );
	glutDialsFunc( NULL );
	glutTabletMotionFunc( NULL );
	glutTabletButtonFunc( NULL );
	glutMenuStateFunc( NULL );
	glutTimerFunc( -1, NULL, 0 );
	glutIdleFunc(Animate);

	// init glew (a window must be open to do this):

#ifdef WIN32
	GLenum err = glewInit( );
	if( err != GLEW_OK )
	{
		fprintf( stderr, "glewInit Error\n" );
	}
	else
		fprintf( stderr, "GLEW initialized OK\n" );
	fprintf( stderr, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
#endif

}


// initialize the display lists that will not change:
// (a display list is a way to store opengl commands in
//  memory so that they can be played back efficiently at a later time
//  with a call to glCallList( )

void
InitLists( )
{
	int i;
	float r = BOXSIZE / 2.f;
	float dx = BOXSIZE / 2.f;
	float dy = BOXSIZE / 2.f;
	float dz = BOXSIZE / 10.f;
	float color[3];

	glutSetWindow( MainWindow );

	// create the object:

	BoxList = glGenLists( 1 );
	glNewList( BoxList, GL_COMPILE );
	glEnable(GL_TEXTURE_2D);

		glBegin( GL_QUADS );

		for(i = 0;i < 100; i++){
			if(i % 2 == 0)
				color[0] = 0.;
			else
				color[0] = 1.;
			if(i % 3 == 0)
				color[1] = 0.;
			else
				color[1] = 1.;
			if(i % 5 == 0)
				color[2] = 0.;
			else
				color[2] = 1.;

			glColor3f( color[0], color[1], color[2] );

			glTexCoord2f(0.4 - i*0.004, 0.4 - i*0.004);
			glVertex3f( -(i / 5.f), -(i / 5.f), -dz - 0.1 * i );
			glTexCoord2f(0.6 + i*0.004, 0.4 - i*0.004);
			glVertex3f( (i / 5.f), -(i / 5.f), -dz - 0.1 * i );
			glTexCoord2f(0.6 + i*0.004, 0.6 + i*0.004);
			glVertex3f( (i / 5.f), (i / 5.f), -dz - 0.1 * i );
			glTexCoord2f(0.4 - i*0.004, 0.6 + i*0.004);
			glVertex3f( -(i / 5.f), (i / 5.f), -dz - 0.1 * i );
		}

		glDisable(GL_TEXTURE_2D);
			/*glColor3f( 0., 0., 1. );
			//glNormal3f( 0., 0.,  1. );
			//	glVertex3f( -dx, -dy,  dz );
			//	glVertex3f(  dx, -dy,  dz );
			//	glVertex3f(  dx,  dy,  dz );
			//	glVertex3f( -dx,  dy,  dz );

			//glNormal3f( 0., 0., -1. );
				//glTexCoord2f( 0., 0. );
				glVertex3f( -dx, -dy, -dz );
				//glTexCoord2f( 0., 1. );
				glVertex3f( -dx,  dy, -dz );
				//glTexCoord2f( 1., 1. );
				glVertex3f(  dx,  dy, -dz );
				//glTexCoord2f( 1., 0. );
				glVertex3f(  dx, -dy, -dz );

			glColor3f( 1., 0., 0. );
			//glNormal3f(  1., 0., 0. );
				glVertex3f(  dx, -dy,  dz );
				glVertex3f(  dx, -dy, -dz );
				glVertex3f(  dx,  dy, -dz );
				glVertex3f(  dx,  dy,  dz );

			//glNormal3f( -1., 0., 0. );
				glVertex3f( -dx, -dy,  dz );
				glVertex3f( -dx,  dy,  dz );
				glVertex3f( -dx,  dy, -dz );
				glVertex3f( -dx, -dy, -dz );

			glColor3f( 0., 1., 0. );
			//glNormal3f( 0.,  1., 0. );
				glVertex3f( -dx,  dy,  dz );
				glVertex3f(  dx,  dy,  dz );
				glVertex3f(  dx,  dy, -dz );
				glVertex3f( -dx,  dy, -dz );

			//glNormal3f( 0., -1., 0. );
				glVertex3f( -dx, -dy,  dz );
				glVertex3f( -dx, -dy, -dz );
				glVertex3f(  dx, -dy, -dz );
				glVertex3f(  dx, -dy,  dz );*/

		glEnd( );

	glEndList( );


	// create the axes:

	AxesList = glGenLists( 1 );
	glNewList( AxesList, GL_COMPILE );
		glLineWidth( AXES_WIDTH );
			Axes( 1.5 );
		glLineWidth( 1. );
	glEndList( );
}


// the keyboard callback:

void
Keyboard( unsigned char c, int x, int y )
{
	if( DebugOn != 0 )
		fprintf( stderr, "Keyboard: '%c' (0x%0x)\n", c, c );

	switch( c )
	{
		case '0':
			Light0On = !Light0On;
		case '1':
			Light1On = !Light1On;
		break;
		case 'f':
		case 'F':
			Freeze = !Freeze;
			if (Freeze)
				glutIdleFunc(NULL);
			else
				glutIdleFunc(Animate);
			break;
		case 'o':
		case 'O':
			WhichProjection = ORTHO;
			break;

		case 'p':
		case 'P':
			WhichProjection = PERSP;
			break;

		case 'q':
		case 'Q':
		case ESCAPE:
			DoMainMenu( QUIT );	// will not return here
			break;				// happy compiler

		default:
			fprintf( stderr, "Don't know what to do with keyboard hit: '%c' (0x%0x)\n", c, c );
	}

	// force a call to Display( ):

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


// called when the mouse button transitions down or up:

void
MouseButton( int button, int state, int x, int y )
{
	int b = 0;			// LEFT, MIDDLE, or RIGHT

	if( DebugOn != 0 )
		fprintf( stderr, "MouseButton: %d, %d, %d, %d\n", button, state, x, y );

	
	// get the proper button bit mask:

	switch( button )
	{
		case GLUT_LEFT_BUTTON:
			b = LEFT;		break;

		case GLUT_MIDDLE_BUTTON:
			b = MIDDLE;		break;

		case GLUT_RIGHT_BUTTON:
			b = RIGHT;		break;

		default:
			b = 0;
			fprintf( stderr, "Unknown mouse button: %d\n", button );
	}


	// button down sets the bit, up clears the bit:

	if( state == GLUT_DOWN )
	{
		Xmouse = x;
		Ymouse = y;
		ActiveButton |= b;		// set the proper bit
	}
	else
	{
		ActiveButton &= ~b;		// clear the proper bit
	}
}


// called when the mouse moves while a button is down:

void
MouseMotion( int x, int y )
{
	if( DebugOn != 0 )
		fprintf( stderr, "MouseMotion: %d, %d\n", x, y );


	int dx = x - Xmouse;		// change in mouse coords
	int dy = y - Ymouse;

	if( ( ActiveButton & LEFT ) != 0 )
	{
		Xrot += ( ANGFACT*dy );
		Yrot += ( ANGFACT*dx );
	}


	if( ( ActiveButton & MIDDLE ) != 0 )
	{
		Scale += SCLFACT * (float) ( dx - dy );

		// keep object from turning inside-out or disappearing:

		if( Scale < MINSCALE )
			Scale = MINSCALE;
	}

	Xmouse = x;			// new current position
	Ymouse = y;

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


// reset the transformations and the colors:
// this only sets the global variables --
// the glut main loop is responsible for redrawing the scene

void
Reset( )
{
	ActiveButton = 0;
	AxesOn = 1;
	DebugOn = 0;
	DepthCueOn = 0;
	Scale  = 1.0;
	WhichColor = WHITE;
	WhichProjection = PERSP;
	Xrot = Yrot = 0.;
}


// called when user resizes the window:

void
Resize( int width, int height )
{
	if( DebugOn != 0 )
		fprintf( stderr, "ReSize: %d, %d\n", width, height );

	// don't really need to do anything since window size is
	// checked each time in Display( ):

	glutSetWindow( MainWindow );
	glutPostRedisplay( );
}


// handle a change to the window's visibility:

void
Visibility ( int state )
{
	if( DebugOn != 0 )
		fprintf( stderr, "Visibility: %d\n", state );

	if( state == GLUT_VISIBLE )
	{
		glutSetWindow( MainWindow );
		glutPostRedisplay( );
	}
	else
	{
		// could optimize by keeping track of the fact
		// that the window is not visible and avoid
		// animating or redrawing it ...
	}
}



///////////////////////////////////////   HANDY UTILITIES:  //////////////////////////


// the stroke characters 'X' 'Y' 'Z' :

static float xx[ ] = {
		0.f, 1.f, 0.f, 1.f
	      };

static float xy[ ] = {
		-.5f, .5f, .5f, -.5f
	      };

static int xorder[ ] = {
		1, 2, -3, 4
		};

static float yx[ ] = {
		0.f, 0.f, -.5f, .5f
	      };

static float yy[ ] = {
		0.f, .6f, 1.f, 1.f
	      };

static int yorder[ ] = {
		1, 2, 3, -2, 4
		};

static float zx[ ] = {
		1.f, 0.f, 1.f, 0.f, .25f, .75f
	      };

static float zy[ ] = {
		.5f, .5f, -.5f, -.5f, 0.f, 0.f
	      };

static int zorder[ ] = {
		1, 2, 3, 4, -5, 6
		};

// fraction of the length to use as height of the characters:
const float LENFRAC = 0.10f;

// fraction of length to use as start location of the characters:
const float BASEFRAC = 1.10f;

//	Draw a set of 3D axes:
//	(length is the axis length in world coordinates)

void
Axes( float length )
{
	glBegin( GL_LINE_STRIP );
		glVertex3f( length, 0., 0. );
		glVertex3f( 0., 0., 0. );
		glVertex3f( 0., length, 0. );
	glEnd( );
	glBegin( GL_LINE_STRIP );
		glVertex3f( 0., 0., 0. );
		glVertex3f( 0., 0., length );
	glEnd( );

	float fact = LENFRAC * length;
	float base = BASEFRAC * length;

	glBegin( GL_LINE_STRIP );
		for( int i = 0; i < 4; i++ )
		{
			int j = xorder[i];
			if( j < 0 )
			{
				
				glEnd( );
				glBegin( GL_LINE_STRIP );
				j = -j;
			}
			j--;
			glVertex3f( base + fact*xx[j], fact*xy[j], 0.0 );
		}
	glEnd( );

	glBegin( GL_LINE_STRIP );
		for( int i = 0; i < 5; i++ )
		{
			int j = yorder[i];
			if( j < 0 )
			{
				
				glEnd( );
				glBegin( GL_LINE_STRIP );
				j = -j;
			}
			j--;
			glVertex3f( fact*yx[j], base + fact*yy[j], 0.0 );
		}
	glEnd( );

	glBegin( GL_LINE_STRIP );
		for( int i = 0; i < 6; i++ )
		{
			int j = zorder[i];
			if( j < 0 )
			{
				
				glEnd( );
				glBegin( GL_LINE_STRIP );
				j = -j;
			}
			j--;
			glVertex3f( 0.0, fact*zy[j], base + fact*zx[j] );
		}
	glEnd( );

}


// function to convert HSV to RGB
// 0.  <=  s, v, r, g, b  <=  1.
// 0.  <= h  <=  360.
// when this returns, call:
//		glColor3fv( rgb );

void
HsvRgb( float hsv[3], float rgb[3] )
{
	// guarantee valid input:

	float h = hsv[0] / 60.f;
	while( h >= 6. )	h -= 6.;
	while( h <  0. ) 	h += 6.;

	float s = hsv[1];
	if( s < 0. )
		s = 0.;
	if( s > 1. )
		s = 1.;

	float v = hsv[2];
	if( v < 0. )
		v = 0.;
	if( v > 1. )
		v = 1.;

	// if sat==0, then is a gray:

	if( s == 0.0 )
	{
		rgb[0] = rgb[1] = rgb[2] = v;
		return;
	}

	// get an rgb from the hue itself:
	
	float i = floor( h );
	float f = h - i;
	float p = v * ( 1.f - s );
	float q = v * ( 1.f - s*f );
	float t = v * ( 1.f - ( s * (1.f-f) ) );

	float r, g, b;			// red, green, blue
	switch( (int) i )
	{
		case 0:
			r = v;	g = t;	b = p;
			break;
	
		case 1:
			r = q;	g = v;	b = p;
			break;
	
		case 2:
			r = p;	g = v;	b = t;
			break;
	
		case 3:
			r = p;	g = q;	b = v;
			break;
	
		case 4:
			r = t;	g = p;	b = v;
			break;
	
		case 5:
			r = v;	g = p;	b = q;
			break;
	}


	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}
