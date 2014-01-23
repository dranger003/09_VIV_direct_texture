varying vec4 color;
uniform sampler2D my_Sampler;
varying vec2 vTexcoor;

void main (void)
{
	vec4 tex = texture2D(my_Sampler, vTexcoor);
	gl_FragColor = tex;
}
