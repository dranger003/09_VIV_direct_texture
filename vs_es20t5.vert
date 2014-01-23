attribute vec4 my_Vertex;
attribute vec4 my_Color;
uniform   mat4 my_TransformMatrix;
attribute vec2 my_Texcoor;
varying vec2 vTexcoor;
varying vec4 color;

void main()
{
	vTexcoor = my_Texcoor;
	color = my_Color;
	gl_Position = my_TransformMatrix * my_Vertex;
}
