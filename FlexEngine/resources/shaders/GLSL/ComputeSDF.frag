#version 400

uniform sampler2D highResTex;
uniform int texChannel;

uniform vec2 charResolution;
uniform float spread;
uniform float highRes; // TODO: Figure out what this is

in vec2 ex_TexCoord;

out vec4 outColor;

void main()
{
	bool insideChar = texture(highResTex, ex_TexCoord).r > 0.5;

	//outColor =  vec4(texture(highResTex, ex_TexCoord.xy).r, 0, 0, 1);

	// Get closest opposite
	vec2 startPos = ex_TexCoord - (vec2(spread) / charResolution);
	vec2 delta = vec2(1.0 / (spread * highRes * 2.0));
	float closest = spread;
	for (int y = 0; y < int(spread * highRes * 2.0); ++y)
	{
		for (int x = 0; x < int(spread * highRes * 2.0); ++x)
		{
			vec2 samplePos = startPos + vec2(x, y) * delta;
			vec2 diff = (ex_TexCoord - samplePos) * charResolution;
			diff.x *= charResolution.x / charResolution.y;
			float dist = length(diff);
			if (dist < closest)
			{
				bool sampleInsideChar = texture(highResTex, samplePos).r > 0.5;
				if (sampleInsideChar != insideChar)
				{
					closest = dist;
				}
			}
		}
	}

	// Bring into range [0, 1]
	float diff = closest / (spread * 2.0);
	float val = 0.5 + (insideChar ? diff : -diff);

	//apply to output color
	vec4 color = vec4(0);
	color[texChannel] = val;
	outColor = color;
}