#version 430

layout (early_fragment_tests) in;

#define MAX_FRAGMENTS 75

in vec3 Position;
in vec3 ModelPosition;
in vec3 Normal;

uniform mat4 ciViewMatrix;

uniform vec4 LightPosition;
uniform vec3 LightIntensity;
uniform vec4 Kd;
uniform vec4 Ka;

struct NodeType {
	vec4 color;
	float depth;
	uint next;
};

layout( binding = 0, r32ui ) uniform uimage2D headPointers;
layout( binding = 0, offset = 0) uniform atomic_uint nextNodeCounter;
layout( binding = 0, std430 ) buffer linkedLists {
	NodeType nodes[];
};
uniform uint MaxNodes;

out vec4 oColor;

subroutine void RenderPassType();
subroutine uniform RenderPassType RenderPass;

vec3 diffuse() {
	vec3 L = normalize( ( ciViewMatrix * LightPosition ).xyz - Position );
	vec3 N = normalize( Normal );
	return LightIntensity * ( Ka.rgb + Kd.rgb * max( dot( N, L ), 0.0 ) );
}

float fresnel() {
	vec3 N = normalize( Normal );
	vec3 V = normalize( -Position );
	return pow( clamp( 1 - dot( N, V ), 0.0, 1.0 ), 2.0 );
}

vec3 stripes() {
	vec3 col;
	float i = floor( ModelPosition.x * 32.0f );
	col.rgb = ( fract( i * 0.5f ) == 0.0f ) ? vec3( 0.37f, 0.81f, 0.0f ) : vec3( 1.0f );
	return col;
}

subroutine(RenderPassType)
void pass1() {
	uint nodeIdx = atomicCounterIncrement( nextNodeCounter );

	if( nodeIdx < MaxNodes ) {
		// update the head pointer for this fragment and retrieve the previous head
		uint prevHead = imageAtomicExchange( headPointers, ivec2( gl_FragCoord.xy ), nodeIdx );

		// append our node to the buffer, update the linked list
		nodes[nodeIdx].color = vec4( diffuse() * ( fresnel() + 0.3 ), Kd.a * ( fresnel() + 0.2 ) );
		nodes[nodeIdx].depth = gl_FragCoord.z;
		nodes[nodeIdx].next = prevHead;
	}
}

subroutine(RenderPassType)
void pass2() {
	NodeType frags[MAX_FRAGMENTS];
	uint pointer = imageLoad( headPointers, ivec2( gl_FragCoord.xy ) ).r;
	int count = 0;

	// Copy the linked list for this fragment into an array
	while( pointer != 0xffffffff && count < MAX_FRAGMENTS ) {
		frags[count] = nodes[pointer];
		pointer = frags[count].next;
		count++;
	}

	// Merge sort the array in decreasing depth
	NodeType leftArray[MAX_FRAGMENTS / 2];
	for( int step = 1; step <= count; step *= 2) {
		for( int i = 0; i < count - step; i += 2 * step ) {
			// merge(step, i, i + step, min(i + step + step, count));
			for( int j = 0; j < step; j++ )
				leftArray[j] = frags[i + j];
			
			int left = 0;
			int right = i + step;
			int c = min( right + step, count );
			for( int j = i; j < c; j++ ) {
				if( right >= c || ( left < step && leftArray[left].depth > frags[right].depth ) )
					frags[j] = leftArray[left++];
				else
					frags[j] = frags[right++];
			}
		}
	}

	// alpha blending
	vec4 color = vec4( 0.0, 0.0, 0.0, 1.0 );
	for( int i = 0; i < count; i++ ) {
		color = mix( color, frags[i].color, frags[i].color.a );
	}

	oColor = color;
}

void main() {
	RenderPass();
}
