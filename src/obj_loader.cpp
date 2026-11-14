#include "obj_loader.h"
#include "math3d.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

// ---------------------------------------------------------------------------
// 路径小工具
// ---------------------------------------------------------------------------
std::string dirOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

std::string joinPath(const std::string& dir, const std::string& name)
{
    if (name.empty()) return std::string();
    // 绝对路径（含盘符或前导斜杠）直接返回
    if (name[0] == '/' || name[0] == '\\') return name;
    if (name.size() > 1 && name[1] == ':') return name;
    return dir + name;
}

// 去掉行尾的 '\r'（本模型的 OBJ 是 CRLF 行尾）
void chompCR(std::string& s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
}

// 把 OBJ 的索引转成 0 基下标。OBJ 允许负数，表示"从末尾往前数"。
inline int resolveIndex(int raw, int count)
{
    if (raw > 0)  return raw - 1;
    if (raw < 0)  return count + raw;
    return -1;      // 0 表示该项不存在
}

// ---------------------------------------------------------------------------
// 解析一个面的顶点项：a | a/b | a//c | a/b/c
// 返回 false 表示这一项格式不认识
// ---------------------------------------------------------------------------
struct CornerRef { int v = 0, vt = 0, vn = 0; };

bool parseCorner(const std::string& tok, CornerRef& out)
{
    // 拆出由 '/' 分隔的至多 3 段
    int  field[3] = { 0, 0, 0 };
    bool present[3] = { false, false, false };
    int  fi = 0;

    const char* p = tok.c_str();
    while (fi < 3)
    {
        // 跳过可能的空段（"a//c" 中间那段为空）
        if (*p == '/') { ++fi; ++p; continue; }

        char* end = nullptr;
        const long val = std::strtol(p, &end, 10);
        if (end == p) break;             // 没有数字了
        field[fi]   = (int)val;
        present[fi] = true;
        p = end;

        if (*p == '/') { ++fi; ++p; continue; }
        break;
    }

    if (!present[0]) return false;
    out.v  = field[0];
    out.vt = present[1] ? field[1] : 0;
    out.vn = present[2] ? field[2] : 0;
    return true;
}

// ---------------------------------------------------------------------------
// 解析 .mtl
// ---------------------------------------------------------------------------
bool parseMTL(const std::string& mtlPath, ObjMaterial& out, std::string& mapOut)
{
    std::ifstream ifs(mtlPath);
    if (!ifs.is_open()) return false;

    const std::string mtlDir = dirOf(mtlPath);

    std::string line;
    while (std::getline(ifs, line))
    {
        chompCR(line);
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line.erase(0, first);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key.empty()) continue;

        if (key == "newmtl")
        {
            ls >> out.name;
        }
        else if (key == "Ka" || key == "Kd" || key == "Ks" || key == "Ke")
        {
            float r = 0, g = 0, b = 0;
            ls >> r >> g >> b;
            float* dst = (key == "Ka") ? out.ambient
                       : (key == "Kd") ? out.diffuse
                       : (key == "Ks") ? out.specular
                                       : out.emission;
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 1.0f;
        }
        else if (key == "Ns")
        {
            float ns = 65.0f;
            ls >> ns;
            // wavefront 的 shininess 范围是 [0,1000]，换算到 OpenGL 的 [0,128]
            out.shininess = ns / 1000.0f * 128.0f;
            if (out.shininess < 0.0f)   out.shininess = 0.0f;
            if (out.shininess > 128.0f) out.shininess = 128.0f;
        }
        else if (key == "map_Kd")
        {
            std::string file;
            ls >> file;
            if (!file.empty()) mapOut = joinPath(mtlDir, file);
        }
    }
    return true;
}

// 去重用的键：OBJ 的 (v, vt, vn) 三元组
struct VertexKey
{
    int v = 0, vt = 0, vn = 0;
    bool operator==(const VertexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};

struct VertexKeyHash
{
    size_t operator()(const VertexKey& k) const noexcept
    {
        // 三个 21 位字段打包（覆盖下标 < 2^21）
        unsigned long long x = (unsigned long long)(unsigned)k.v
                             | ((unsigned long long)(unsigned)k.vt << 21)
                             | ((unsigned long long)(unsigned)k.vn << 42);
        x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return (size_t)x;
    }
};

} // namespace

// ---------------------------------------------------------------------------
bool loadOBJ(const std::string& path, MeshData& out, std::string* errOut)
{
    auto fail = [&](const std::string& msg) {
        if (errOut) *errOut = msg;
        return false;
    };

    std::ifstream ifs(path);
    if (!ifs.is_open()) return fail("cannot open OBJ file: " + path);

    const std::string objDir = dirOf(path);

    // 原始数据（OBJ 的独立索引空间）
    struct RawVert { float x, y, z; };
    struct RawUV   { float u, v; };
    std::vector<RawVert> V;
    std::vector<RawUV>   T;
    std::vector<RawVert> N;

    // 去重后的统一顶点缓冲
    std::unordered_map<VertexKey, GLuint, VertexKeyHash> unique;
    unique.reserve(160000);

    std::string curMaterial;
    std::string mtlFile;
    bool sawFace = false;
    std::vector<int> sourceVertices;
    std::vector<bool> missingNormals;

    std::string line;
    while (std::getline(ifs, line))
    {
        chompCR(line);
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        line.erase(0, first);
        if (line.empty() || line[0] == '#') continue;

        // 按首字符分派（与指导书 parseStream 的思路一致）
        switch (line[0])
        {
        case 'v':
            if (line.size() > 1 && line[1] == 't')          // vt u v
            {
                std::istringstream ls(line);
                std::string k; RawUV t{ 0, 0 };
                ls >> k >> t.u >> t.v;
                T.push_back(t);
            }
            else if (line.size() > 1 && line[1] == 'n')     // vn x y z
            {
                std::istringstream ls(line);
                std::string k; RawVert n{ 0, 0, 0 };
                ls >> k >> n.x >> n.y >> n.z;
                N.push_back(n);
            }
            else                                            // v x y z
            {
                std::istringstream ls(line);
                std::string k; RawVert p{ 0, 0, 0 };
                ls >> k >> p.x >> p.y >> p.z;
                V.push_back(p);
            }
            break;

        case 'f':
        {
            sawFace = true;
            std::istringstream ls(line);
            std::string k;
            ls >> k;

            // 收集所有角点
            std::vector<CornerRef> corners;
            std::string tok;
            while (ls >> tok)
            {
                CornerRef c;
                if (tok[0] == '#') break;
                if (!parseCorner(tok, c)) return fail("invalid face corner: " + tok);
                const bool hasUV = c.vt != 0, hasNormal = c.vn != 0;
                c.v  = resolveIndex(c.v,  (int)V.size());
                c.vt = resolveIndex(c.vt, (int)T.size());
                c.vn = resolveIndex(c.vn, (int)N.size());
                if (c.v < 0 || c.v >= (int)V.size() ||
                    (hasUV && (c.vt < 0 || c.vt >= (int)T.size())) ||
                    (hasNormal && (c.vn < 0 || c.vn >= (int)N.size())))
                    return fail("face index out of range: " + tok);
                corners.push_back(c);
            }
            if (corners.size() < 3) return fail("face has fewer than three corners");

            if (corners.size() == 3)      out.srcFaceCount += 1;
            else if (corners.size() == 4) { out.srcFaceCount += 1; out.quadFaceCount += 1; }
            else                          { out.srcFaceCount += 1; out.otherFaceCount += 1; }

            // 把每个角点映射到统一顶点缓冲里的下标
            std::vector<GLuint> ci(corners.size());
            for (size_t i = 0; i < corners.size(); ++i)
            {
                const CornerRef& c = corners[i];
                const VertexKey key{ c.v, c.vt, c.vn };

                auto it = unique.find(key);
                if (it == unique.end())
                {
                    const GLuint id = (GLuint)(out.positions.size() / 3);
                    sourceVertices.push_back(c.v);
                    missingNormals.push_back(c.vn < 0);

                    if (c.v >= 0 && c.v < (int)V.size())
                        out.positions.insert(out.positions.end(),
                            { V[c.v].x, V[c.v].y, V[c.v].z });
                    else
                        out.positions.insert(out.positions.end(), { 0.0f, 0.0f, 0.0f });

                    if (c.vn >= 0 && c.vn < (int)N.size())
                    {
                        out.normals.insert(out.normals.end(),
                            { N[c.vn].x, N[c.vn].y, N[c.vn].z });
                        out.hasNormals = true;
                    }
                    else
                    {
                        out.normals.insert(out.normals.end(), { 0.0f, 0.0f, 1.0f });
                    }

                    if (c.vt >= 0 && c.vt < (int)T.size())
                    {
                        out.uvs.insert(out.uvs.end(), { T[c.vt].u, T[c.vt].v });
                        out.hasUVs = true;
                    }
                    else
                    {
                        out.uvs.insert(out.uvs.end(), { 0.0f, 0.0f });
                    }

                    it = unique.emplace(key, id).first;
                }
                ci[i] = it->second;
            }

            // 扇形三角化 (0,1,2) (0,2,3) ... —— 保持 OBJ 的 CCW 绕序
            for (size_t i = 1; i + 1 < ci.size(); ++i)
            {
                out.indices.push_back(ci[0]);
                out.indices.push_back(ci[i]);
                out.indices.push_back(ci[i + 1]);
            }
            break;
        }

        case 'm':
            if (line.rfind("mtllib", 0) == 0)
            {
                std::istringstream ls(line);
                std::string k;
                ls >> k >> mtlFile;
            }
            break;

        case 'u':
            if (line.rfind("usemtl", 0) == 0)
            {
                std::istringstream ls(line);
                std::string k;
                ls >> k >> curMaterial;
                if (out.material.name.empty()) out.material.name = curMaterial;
            }
            break;

        default:
            break;      // o / g / s 等暂不处理
        }
    }

    if (!sawFace || out.indices.empty()) return fail("no face data in OBJ: " + path);
    // Area-weighted normals for missing vn entries; accumulate by source position
    // so a UV seam does not introduce a lighting seam. Authored normals survive.
    std::vector<Vec3> sums(V.size());
    for (size_t i=0; i<out.indices.size(); i+=3) {
        GLuint a=out.indices[i], b=out.indices[i+1], c=out.indices[i+2];
        auto point=[&](GLuint v) { return v3(out.positions[v*3],out.positions[v*3+1],out.positions[v*3+2]); };
        Vec3 n=v3cross(point(b)-point(a),point(c)-point(a));
        for (GLuint v : {a,b,c}) sums[sourceVertices[v]]=sums[sourceVertices[v]]+n;
    }
    for (size_t i=0;i<missingNormals.size();++i) if (missingNormals[i]) {
        Vec3 n=v3norm(sums[sourceVertices[i]]);
        if(v3len(n)<1e-8f) n=v3(0,1,0);
        out.normals[i*3]=n.x; out.normals[i*3+1]=n.y; out.normals[i*3+2]=n.z;
    }
    out.hasNormals=true;

    // ---- 材质与贴图 ----
    if (!mtlFile.empty())
    {
        ObjMaterial mtl;
        std::string mapPath;
        if (parseMTL(joinPath(objDir, mtlFile), mtl, mapPath))
        {
            out.material      = mtl;
            out.diffuseMapPath = mapPath;
        }
    }

    // ---- 包围盒 / 包围球（供归一化使用）----
    const size_t nv = out.positions.size() / 3;
    if (nv == 0) return fail("no vertices in OBJ: " + path);

    float mn[3] = { out.positions[0], out.positions[1], out.positions[2] };
    float mx[3] = { mn[0], mn[1], mn[2] };
    for (size_t i = 1; i < nv; ++i)
    {
        for (int a = 0; a < 3; ++a)
        {
            const float v = out.positions[i * 3 + a];
            if (v < mn[a]) mn[a] = v;
            if (v > mx[a]) mx[a] = v;
        }
    }
    for (int a = 0; a < 3; ++a)
    {
        out.bboxMin[a] = mn[a];
        out.bboxMax[a] = mx[a];
        out.center[a]  = (mn[a] + mx[a]) * 0.5f;
    }

    float maxDistSq = 0.0f;
    for (size_t i = 0; i < nv; ++i)
    {
        const float dx = out.positions[i * 3 + 0] - out.center[0];
        const float dy = out.positions[i * 3 + 1] - out.center[1];
        const float dz = out.positions[i * 3 + 2] - out.center[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > maxDistSq) maxDistSq = d2;
    }
    out.radius = std::sqrt(maxDistSq);
    if (out.radius < 1e-6f) out.radius = 1.0f;

    out.uniqueVertexCount = (int)nv;
    out.triangleCount     = (int)(out.indices.size() / 3);
    out.srcVertexCount    = (int)V.size();
    out.valid             = true;
    return true;
}
