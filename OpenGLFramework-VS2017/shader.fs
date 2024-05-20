#version 330 core

out vec4 FragColor;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 vertex_view;

struct PhongMaterial
{
	vec3 Ka;
	vec3 Kd;
	vec3 Ks;
};

struct Light
{
	vec3 position;
	vec3 spotDirection;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
};

uniform int cur_light_mode;
uniform mat4 view_matrix;
uniform Light light[3];
uniform PhongMaterial material;
uniform float shininess;
uniform int is_per_pixel_lighting;

vec4 lightInView;
vec3 L, H;

void main()
{
	// [TODO]
	vec3 N = normalize(vertex_normal);
	vec3 V = -vertex_view;
	vec3 frag_color;

	// Directional light
	if (cur_light_mode == 0)
	{
		lightInView = view_matrix * vec4(light[0].position, 1.0f);
		L = normalize(lightInView.xyz + V);
		H = normalize(L + V);

		vec3 ambient = light[0].ambient * material.Ka;
		vec3 diffuse = light[0].diffuse * max(dot(L, N), 0.0) * material.Kd;
		vec3 specular = light[0].specular * pow(max(dot(H, N), 0.0), shininess) * material.Ks;

		frag_color = ambient + diffuse + specular;
	}

	// Point light
	if (cur_light_mode == 1)
	{
		lightInView = view_matrix * vec4(light[1].position, 1.0f);
		L = normalize(lightInView.xyz + V);
		H = normalize(L + V);

		float dis = length(lightInView.xyz + V);
		float attenuation = light[1].constantAttenuation +
							light[1].linearAttenuation * dis +
							light[1].quadraticAttenuation * pow(dis, 2);
		float f = 1.0f / attenuation;

		vec3 ambient = light[1].ambient * material.Ka;
		vec3 diffuse = light[1].diffuse * max(dot(L, N), 0.0) * material.Kd;
		vec3 specular = light[1].specular * pow(max(dot(H, N), 0.0), shininess) * material.Ks;

		frag_color = ambient + f * (diffuse + specular);
	}

	// Spot light
	if (cur_light_mode == 2)
	{
		lightInView = view_matrix * vec4(light[2].position, 1.0f);
		L = normalize(lightInView.xyz + V);
		H = normalize(L + V);

		float spot = dot(-L, normalize(light[2].spotDirection.xyz));
		float dis = length(lightInView.xyz - V);
		float attenuation = light[2].constantAttenuation +
							light[2].linearAttenuation * dis +
							light[2].quadraticAttenuation * pow(dis, 2);
		float f = 1.0f / attenuation;

		vec3 ambient = light[2].ambient * material.Ka;
		vec3 diffuse = light[2].diffuse * max(dot(L, N), 0.0) * material.Kd;
		vec3 specular = light[2].specular * pow(max(dot(H, N), 0.0), shininess) * material.Ks;

		frag_color = ambient + f * (spot < light[2].spotCutoff ? 0 : pow(max(spot, 0), light[2].spotExponent)) * (diffuse + specular);
	}

	FragColor = (is_per_pixel_lighting == 0) ? vec4(vertex_color, 1.0f) : vec4(frag_color, 1.0f);
}
