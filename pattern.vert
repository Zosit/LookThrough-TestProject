#version 330 compatibility
uniform float uX0; // object color
uniform float uT0; // object color

out vec3 vColor;
void
main( )
{
vec4 pos = gl_Vertex;
if(((pos.x - (cos(pos.y * 10) / 4.)) > (uT0 - 0.4)) && ((pos.x + (cos(pos.y * 10) / 4.)) < (uT0 + 0.4))){
vColor[0] = (uT0 / 4. + 0.5); // set rgb from xyz!
vColor[1] = (uT0 / 4. + 0.5); // set rgb from xyz!
vColor[2] = (pos.y + 0.5); // set rgb from xyz!
} else if(((pos.y) > (uT0 - 0.2)) && ((pos.y) < (uT0 + 0.2))){
vColor[0] = (pos.y + 0.5); // set rgb from xyz!
vColor[1] = (0.5); // set rgb from xyz!
vColor[2] = (pos.y + 0.5); // set rgb from xyz!
} else{
vColor[0] = 0; // set rgb from xyz!
vColor[1] = 0; // set rgb from xyz!
vColor[2] = 0; // set rgb from xyz!
}

gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}