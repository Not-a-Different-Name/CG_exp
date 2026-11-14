#include "obj_loader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
int main() {
 const auto file=std::filesystem::temp_directory_path()/"cg_obj_loader_regression.obj";
 int failures=0;
 auto test=[&](const char* name,const char* body,bool expected,bool checkNormal=false) {
  {std::ofstream out(file);out<<body;}
  MeshData data; std::string error;
  bool loaded=loadOBJ(file.string(),data,&error);
  bool pass=loaded==expected;
  if(pass && loaded && checkNormal) {
   pass=data.hasNormals;
   for(size_t i=0;i<data.normals.size();i+=3)
    pass=pass && std::fabs(data.normals[i])<1e-5f && std::fabs(data.normals[i+1])<1e-5f && std::fabs(data.normals[i+2]-1.0f)<1e-5f;
  }
  std::cout<<(pass?"PASS ":"FAIL ")<<name<<"\n"; failures+=!pass;
 };
 test("missing normals", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",true,true);
 test("whitespace / negative indices / comment", "  v 0 0 0\n\tv 1 0 0\nv 0 1 0\n f -3 -2 -1 # triangle\n",true,true);
 test("mixed authored and generated normals", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2 3\n",true,true);
 test("invalid position index", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n",false);
 test("invalid normal index", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//2 2 3\n",false);
 test("short face", "v 0 0 0\nv 1 0 0\nf 1 2\n",false);
 test("empty model", "# no geometry\n",false);
 test("quad triangulation", "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n",true,true);
 std::filesystem::remove(file);
 return failures?1:0;
}
