xof 0303txt 0032
template Frame {
 <3d82ab46-62da-11cf-ab39-0020af71e433>
 [...]
}

template Matrix4x4 {
 <f6f23f45-7686-11cf-8f52-0040333594a3>
 array FLOAT matrix[16];
}

template FrameTransformMatrix {
 <f6f23f41-7686-11cf-8f52-0040333594a3>
 Matrix4x4 frameMatrix;
}

template Vector {
 <3d82ab5e-62da-11cf-ab39-0020af71e433>
 FLOAT x;
 FLOAT y;
 FLOAT z;
}

template MeshFace {
 <3d82ab5f-62da-11cf-ab39-0020af71e433>
 DWORD nFaceVertexIndices;
 array DWORD faceVertexIndices[nFaceVertexIndices];
}

template Mesh {
 <3d82ab44-62da-11cf-ab39-0020af71e433>
 DWORD nVertices;
 array Vector vertices[nVertices];
 DWORD nFaces;
 array MeshFace faces[nFaces];
 [...]
}

template MeshNormals {
 <f6f23f43-7686-11cf-8f52-0040333594a3>
 DWORD nNormals;
 array Vector normals[nNormals];
 DWORD nFaceNormals;
 array MeshFace faceNormals[nFaceNormals];
}


Frame Cube {
 

 FrameTransformMatrix {
  1.000000,0.000000,0.000000,0.000000,0.000000,1.000000,0.000000,0.000000,0.000000,0.000000,1.000000,0.000000,0.000000,0.000000,0.000000,1.000000;;
 }

 Mesh {
  8;
  1.000000;1.000000;1.000000;,
  1.000000;1.000000;-1.000000;,
  1.000000;-1.000000;1.000000;,
  1.000000;-1.000000;-1.000000;,
  -1.000000;1.000000;1.000000;,
  -1.000000;1.000000;-1.000000;,
  -1.000000;-1.000000;1.000000;,
  -1.000000;-1.000000;-1.000000;;
  12;
  3;2,3,1;,
  3;2,1,0;,
  3;0,1,5;,
  3;0,5,4;,
  3;4,5,7;,
  3;4,7,6;,
  3;6,7,3;,
  3;6,3,2;,
  3;1,3,7;,
  3;1,7,5;,
  3;2,0,4;,
  3;2,4,6;;

  MeshNormals {
   8;
   0.408248;0.816497;0.408248;,
   0.666667;0.333333;-0.666667;,
   0.666667;-0.333333;0.666667;,
   0.408248;-0.816497;-0.408248;,
   -0.666667;0.333333;0.666667;,
   -0.408248;0.816497;-0.408248;,
   -0.408248;-0.816497;0.408248;,
   -0.666667;-0.333333;-0.666667;;
   12;
   3;2,3,1;,
   3;2,1,0;,
   3;0,1,5;,
   3;0,5,4;,
   3;4,5,7;,
   3;4,7,6;,
   3;6,7,3;,
   3;6,3,2;,
   3;1,3,7;,
   3;1,7,5;,
   3;2,0,4;,
   3;2,4,6;;
  }
 }
}