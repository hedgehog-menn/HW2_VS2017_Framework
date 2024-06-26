#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

using namespace std;

// Default window size
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 800;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

// My additional useful values & function
const float PI = (float)atan(1) * 4;
inline float degree_to_radian(double degree)
{
	return (float)(degree * PI / 180);
}
int is_per_pixel_lighting = 0;
GLfloat shininess;

enum LightMode
{
	DirectionalLight = 0,
	PointLight = 1,
	SpotLight = 2
};

struct iLocLight
{
	GLuint position;
	GLuint ambient;
	GLuint diffuse;
	GLuint specular;
	GLuint spotDirection;
	GLuint spotCutoff;
	GLuint spotExponent;
	GLuint constantAttenuation;
	GLuint linearAttenuation;
	GLuint quadraticAttenuation;
} iLocLight[3];

struct Light
{
	Vector3 position;
	Vector3 ambient;
	Vector3 diffuse;
	Vector3 specular;
	Vector3 spotDirection;
	GLfloat spotCutoff;
	GLfloat spotExponent;
	GLfloat constantAttenuation;
	GLfloat linearAttenuation;
	GLfloat quadraticAttenuation;
} light[3];

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	LightEdit = 3,
	ShininessEdit = 4,
};

struct Uniform
{
	GLint iLocMVP;
	GLint iLocM;
	GLint iLocV;
	GLint Ka;
	GLint Kd;
	GLint Ks;
	GLint LightMode;
	GLint Shininess;
};
Uniform uniform;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0); // Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

TransMode cur_trans_mode = GeoTranslation;
LightMode cur_light_mode = DirectionalLight;

Matrix4 view_matrix;
Matrix4 project_matrix;

int cur_idx = 0; // represent which model should be rendered now

static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}

// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x) * rotateY(vec.y) * rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	Vector3 Z = (main_camera.center - main_camera.position);
	Z.normalize();
	Vector3 X = Z.cross(main_camera.up_vector);
	X.normalize();
	Vector3 Y = X.cross(Z).normalize();

	Matrix4 R = Matrix4(
		X.x, X.y, X.z, 0,
		Y.x, Y.y, Y.z, 0,
		-Z.x, -Z.y, -Z.z, 0,
		0, 0, 0, 1);

	Matrix4 T = Matrix4(
		1, 0, 0, -main_camera.position.x,
		0, 1, 0, -main_camera.position.y,
		0, 0, 1, -main_camera.position.z,
		0, 0, 0, 1);

	view_matrix = R * T;
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	float f = 1 / tan(degree_to_radian(proj.fovy) / 2);
	float z = (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip);
	float t_z = (2 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip);

	project_matrix = Matrix4(
		f, 0, 0, 0,
		0, f, 0, 0,
		0, 0, z, t_z,
		0, 0, -1, 0);

	if (proj.aspect >= 1)
	{
		project_matrix[0] = f / (proj.aspect / 2);
	}
	else
	{
		project_matrix[5] = f * proj.aspect;
	}
}

void setGLMatrix(GLfloat *glm, Matrix4 &m)
{
	glm[0] = m[0];
	glm[4] = m[1];
	glm[8] = m[2];
	glm[12] = m[3];
	glm[1] = m[4];
	glm[5] = m[5];
	glm[9] = m[6];
	glm[13] = m[7];
	glm[2] = m[8];
	glm[6] = m[9];
	glm[10] = m[10];
	glm[14] = m[11];
	glm[3] = m[12];
	glm[7] = m[13];
	glm[11] = m[14];
	glm[15] = m[15];
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
	// Prevent divided by 0
	if (height == 0)
	{
		return;
	}

	proj.aspect = (float)width / (float)height;
	WINDOW_WIDTH = width;
	WINDOW_HEIGHT = height;

	// Perspective mode only
	float f = 1 / tan(degree_to_radian(proj.fovy) / 2);

	if (proj.aspect >= 1)
	{
		project_matrix[0] = f / (proj.aspect / 2);
	}
	else
	{
		project_matrix[5] = f * proj.aspect;
	}
}

// Render function for display rendering
void RenderScene(void)
{
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP, model_matrix;
	GLfloat mvp[16], m[16], v[16];

	// [TODO] multiply all the matrix
	model_matrix = T * R * S;
	MVP = project_matrix * view_matrix * model_matrix;
	// row-major ---> column-major
	setGLMatrix(mvp, MVP);
	setGLMatrix(m, model_matrix);
	setGLMatrix(v, view_matrix);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(uniform.iLocMVP, 1, GL_FALSE, mvp);
	// use uniform to send view_matrix to vertex shader
	glUniformMatrix4fv(uniform.iLocV, 1, GL_FALSE, v);
	// use uniform to send model_matrix to vertex shader
	glUniformMatrix4fv(uniform.iLocM, 1, GL_FALSE, m);

	// Update light detail
	glUniform1i(uniform.LightMode, cur_light_mode);
	glUniform1f(uniform.Shininess, shininess);
	for (int i = 0; i <= 2; i++)
	{
		glUniform3fv(iLocLight[i].ambient, 1, &light[i].ambient[0]);
		glUniform3fv(iLocLight[i].diffuse, 1, &light[i].diffuse[0]);
		glUniform3fv(iLocLight[i].specular, 1, &light[i].specular[0]);
	}
	glUniform3fv(iLocLight[0].position, 1, &light[0].position[0]);
	glUniform3fv(iLocLight[1].position, 1, &light[1].position[0]);
	glUniform1f(iLocLight[1].constantAttenuation, light[1].constantAttenuation);
	glUniform1f(iLocLight[1].linearAttenuation, light[1].linearAttenuation);
	glUniform1f(iLocLight[1].quadraticAttenuation, light[1].quadraticAttenuation);
	glUniform3fv(iLocLight[2].position, 1, &light[2].position[0]);
	glUniform3fv(iLocLight[2].spotDirection, 1, &light[2].spotDirection[0]);
	glUniform1f(iLocLight[2].spotExponent, light[2].spotExponent);
	glUniform1f(iLocLight[2].spotCutoff, light[2].spotCutoff);
	glUniform1f(iLocLight[2].constantAttenuation, light[2].constantAttenuation);
	glUniform1f(iLocLight[2].linearAttenuation, light[2].linearAttenuation);
	glUniform1f(iLocLight[2].quadraticAttenuation, light[2].quadraticAttenuation);

	for (int i = 0; i < models[cur_idx].shapes.size(); i++)
	{
		// set glViewport and draw twice ...
		glUniform3fv(uniform.Ka, 1, &models[cur_idx].shapes[i].material.Ka[0]);
		glUniform3fv(uniform.Kd, 1, &models[cur_idx].shapes[i].material.Kd[0]);
		glUniform3fv(uniform.Ks, 1, &models[cur_idx].shapes[i].material.Ks[0]);

		/* draw left */
		glUniform1i(is_per_pixel_lighting, 0);
		glViewport(0, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);

		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);

		/* draw right */
		glUniform1i(is_per_pixel_lighting, 1);
		glViewport(WINDOW_WIDTH / 2, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);

		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard

	// fix duplicate execute
	if (action != GLFW_PRESS)
	{
		return;
	}

	switch (key)
	{
	case GLFW_KEY_Z:
		cur_idx = cur_idx == 0 ? models.size() - 1 : cur_idx - 1;
		std::cout << "Model " << cur_idx + 1 << " is selected.\n";
		break;
	case GLFW_KEY_X:
		cur_idx = cur_idx == models.size() - 1 ? 0 : cur_idx + 1;
		std::cout << "Model " << cur_idx + 1 << " is selected.\n";
		break;
	case GLFW_KEY_LEFT:
		cur_idx = cur_idx == 0 ? models.size() - 1 : cur_idx - 1;
		std::cout << "Model " << cur_idx + 1 << " is selected.\n";
		break;
	case GLFW_KEY_RIGHT:
		cur_idx = cur_idx == models.size() - 1 ? 0 : cur_idx + 1;
		std::cout << "Model " << cur_idx + 1 << " is selected.\n";
		break;
	case GLFW_KEY_T:
		cur_trans_mode = GeoTranslation;
		break;
	case GLFW_KEY_S:
		cur_trans_mode = GeoScaling;
		break;
	case GLFW_KEY_R:
		cur_trans_mode = GeoRotation;
		break;
	case GLFW_KEY_L:
		// Change Light Mode
		switch (cur_light_mode)
		{
		case DirectionalLight:
			cur_light_mode = PointLight;
			std::cout << "Light mode: " << "Point light\n";
			break;
		case PointLight:
			cur_light_mode = SpotLight;
			std::cout << "Light mode: " << "Spot light\n";
			break;
		case SpotLight:
			cur_light_mode = DirectionalLight;
			std::cout << "Light mode: " << "Directional light\n";
			break;
		default:
			break;
		}
		break;
	case GLFW_KEY_K:
		cur_trans_mode = LightEdit;
		break;
	case GLFW_KEY_J:
		cur_trans_mode = ShininessEdit;
		break;
	default:
		break;
	}
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negative
	switch (cur_trans_mode)
	{
	case GeoTranslation:
		models[cur_idx].position.z += (float)yoffset / 10.0f;
		break;
	case GeoScaling:
		models[cur_idx].scale.z += (float)yoffset / 10.0f;
		break;
	case GeoRotation:
		models[cur_idx].rotation.z += (float)yoffset / 10.0f;
		break;

	case LightEdit:
		if (cur_light_mode == DirectionalLight || cur_light_mode == PointLight)
		{
			light[cur_light_mode].diffuse += Vector3(0.1f, 0.1f, 0.1f) * (float)yoffset;
		}
		if (cur_light_mode == SpotLight)
		{
			if (light[2].spotCutoff <= 0 && yoffset > 0)
			{
				break;
			}
			else if (light[2].spotCutoff >= degree_to_radian(90.0) && (float)yoffset < 0)
			{
				break;
			}
			else
			{
				light[2].spotCutoff -= (float)yoffset / 150.0f;
			}
		}
		break;
	case ShininessEdit:
		shininess -= (float)yoffset * 5.0f;
		break;
	default:
		break;
	}
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		mouse_pressed = true;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		mouse_pressed = false;
	}
}

float current_press_x = (float)starting_press_x;
float current_press_y = (float)starting_press_y;

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	float dif_x = xpos - current_press_x;
	float dif_y = current_press_y - ypos;
	current_press_x = xpos;
	current_press_y = ypos;

	// execute while only holding (left-mouse) clicked
	if (!mouse_pressed)
	{
		return;
	}

	switch (cur_trans_mode)
	{
	case GeoTranslation:
		models[cur_idx].position.x += dif_x / 100.0f;
		models[cur_idx].position.y += dif_y / 100.0f;
		break;
	case GeoScaling:
		models[cur_idx].scale.x += dif_x / 100.0f;
		models[cur_idx].scale.y += dif_y / 100.0f;
		break;
	case GeoRotation:
		models[cur_idx].rotation.x += PI / 180.0f * dif_y;
		models[cur_idx].rotation.y -= PI / 180.0f * dif_x;

		// Alternative rotating direction
		// models[cur_idx].rotation.x -= PI / 180 * dif_y;
		// models[cur_idx].rotation.y += PI / 180 * dif_x;
		break;
	case LightEdit:
		light[cur_light_mode].position[0] += dif_x / 250.0f;
		light[cur_light_mode].position[1] += dif_y / 250.0f;
		break;
	default:
		return;
	}
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar **)&vs, NULL);
	glShaderSource(f, 1, (const GLchar **)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n"
				  << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n"
				  << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n"
				  << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	uniform.iLocMVP = glGetUniformLocation(p, "mvp");
	uniform.iLocV = glGetUniformLocation(p, "view_matrix");
	uniform.iLocM = glGetUniformLocation(p, "model_matrix");

	uniform.Ka = glGetUniformLocation(p, "material.Ka");
	uniform.Kd = glGetUniformLocation(p, "material.Kd");
	uniform.Ks = glGetUniformLocation(p, "material.Ks");

	uniform.LightMode = glGetUniformLocation(p, "cur_light_mode");
	uniform.Shininess = glGetUniformLocation(p, "shininess");
	is_per_pixel_lighting = glGetUniformLocation(p, "is_per_pixel_lighting");

	// Directional/Point/Spot light
	for (int i = 0; i <= 2; i++)
	{
		iLocLight[i].position = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].position").c_str());
		iLocLight[i].ambient = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].ambient").c_str());
		iLocLight[i].diffuse = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].diffuse").c_str());
		iLocLight[i].specular = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].specular").c_str());
		iLocLight[i].constantAttenuation = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].constantAttenuation").c_str());
		iLocLight[i].linearAttenuation = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].linearAttenuation").c_str());
		iLocLight[i].quadraticAttenuation = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].quadraticAttenuation").c_str());
		iLocLight[i].spotDirection = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].spotDirection").c_str());
		iLocLight[i].spotCutoff = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].spotCutoff").c_str());
		iLocLight[i].spotExponent = glGetUniformLocation(p, ("light[" + std::to_string(i) + "].spotExponent").c_str());
	}

	if (success)
		glUseProgram(p);
	else
	{
		system("pause");
		exit(123);
	}
}

void normalization(tinyobj::attrib_t *attrib, vector<GLfloat> &vertices, vector<GLfloat> &colors, vector<GLfloat> &normals, tinyobj::shape_t *shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		// maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		// std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++)
	{
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++)
		{
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0)
			{
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string &filepath)
{
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty())
	{
		cout << warn << std::endl;
	}

	if (!err.empty())
	{
		cerr << err << std::endl;
	}

	if (!ret)
	{
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", int(shapes.size()), int(materials.size()));
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	// [TODO] Setup some parameters if you need
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	// Directional light
	light[0].position = Vector3(1.0f, 1.0f, 1.0f);
	light[0].ambient = Vector3(0.15f, 0.15f, 0.15f);
	light[0].diffuse = Vector3(1.0f, 1.0f, 1.0f);
	light[0].specular = Vector3(1.0f, 1.0f, 1.0f);
	light[0].constantAttenuation = 0.0f;
	light[0].linearAttenuation = 0.0f;
	light[0].quadraticAttenuation = 0.0f;

	// Point light
	light[1].position = Vector3(0.0f, 2.0f, 1.0f);
	light[1].ambient = Vector3(0.15f, 0.15f, 0.15f);
	light[1].diffuse = Vector3(1.0f, 1.0f, 1.0f);
	light[1].specular = Vector3(1.0f, 1.0f, 1.0f);
	light[1].constantAttenuation = 0.01f;
	light[1].linearAttenuation = 0.8f;
	light[1].quadraticAttenuation = 0.1f;

	// Spot light
	light[2].position = Vector3(0.0f, 0.0f, 2.0f);
	light[2].ambient = Vector3(0.15f, 0.15f, 0.15f);
	light[2].diffuse = Vector3(1.0f, 1.0f, 1.0f);
	light[2].specular = Vector3(1.0f, 1.0f, 1.0f);
	light[2].constantAttenuation = 0.05f;
	light[2].linearAttenuation = 0.3f;
	light[2].quadraticAttenuation = 0.6f;

	light[2].spotDirection = Vector3(0.0f, 0.0f, -1.0f);
	light[2].spotExponent = 50.0f;
	light[2].spotCutoff = degree_to_radian(30.0);

	shininess = 64.0f;

	setViewingMatrix();
	setPerspective(); // set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{"../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	// [TODO] Load five model at here
	for (int i = 0; i < model_list.size(); i++)
	{
		LoadModels(model_list[i]);
	}
	std::cout << "Model " << cur_idx + 1 << " is selected.\n";
	std::cout << "Light mode: " << "Directional light\n";
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char *)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char *)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char *)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char *)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}

int main(int argc, char **argv)
{
	// initial glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

	// create window
	GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "112065431 ID HW2", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// load OpenGL function pointer
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// register glfw callback functions
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

	glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
	while (!glfwWindowShouldClose(window))
	{
		// render
		RenderScene();

		// swap buffer from back to front
		glfwSwapBuffers(window);

		// Poll input event
		glfwPollEvents();
	}

	// just for compatibiliy purposes
	return 0;
}
