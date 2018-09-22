#version 430

in vec4 ciPosition;
in vec3 ciNormal;

out vec3 Position;
out vec3 ModelPosition;
out vec3 Normal;

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;
uniform mat4 ciViewMatrix;
uniform mat3 ciNormalMatrix;

void main() {
    Normal = normalize( ciNormalMatrix * ciNormal );
    Position = vec3( ciModelView * ciPosition );
	ModelPosition = ciPosition.xyz;

    gl_Position = ciModelViewProjection * ciPosition;
}
