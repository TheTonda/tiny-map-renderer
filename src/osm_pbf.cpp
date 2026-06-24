#include "osm_pbf.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>
#include <zlib.h>

// ============================================================
// Minimal protobuf wire-format parser
// ============================================================

namespace {

bool read_varint(const uint8_t*& p, const uint8_t* end, uint64_t& out) {
    out = 0;
    int shift = 0;
    while (p < end) {
        uint8_t b = *p++;
        out |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
        shift += 7;
    }
    return false;
}

inline int64_t zigzag64(uint64_t n) { return static_cast<int64_t>((n >> 1) ^ -(n & 1)); }

bool skip_field(const uint8_t*& p, const uint8_t* end, int wire_type) {
    switch (wire_type) {
    case 0: while (p < end && (*p & 0x80)) p++; if (p < end) p++; return true;
    case 1: if (p + 8 > end) return false; p += 8; return true;
    case 2: { uint64_t len; if (!read_varint(p, end, len)) return false;
              if (static_cast<size_t>(len) > static_cast<size_t>(end - p)) return false;
              p += static_cast<size_t>(len); return true; }
    case 5: if (p + 4 > end) return false; p += 4; return true;
    default: return false;
    }
}

bool read_length_delimited(const uint8_t*& p, const uint8_t* end,
                           const uint8_t*& data, size_t& len) {
    uint64_t vlen;
    if (!read_varint(p, end, vlen)) return false;
    len = static_cast<size_t>(vlen);
    if (len > static_cast<size_t>(end - p)) return false;
    data = p;
    p += len;
    return true;
}

bool read_tag(const uint8_t*& p, const uint8_t* end, uint32_t& field, int& wire) {
    uint64_t tag;
    if (!read_varint(p, end, tag)) return false;
    field = static_cast<uint32_t>(tag >> 3);
    wire = static_cast<int>(tag & 7);
    return true;
}

// ============================================================
// Zlib decompression
// ============================================================

bool zlib_decompress(const uint8_t* in, size_t in_len,
                     std::vector<uint8_t>& out, size_t expected_size) {
    out.clear();
    out.resize(expected_size > 0 ? expected_size : in_len * 4);

    z_stream s{};
    s.next_in = const_cast<uint8_t*>(in);
    s.avail_in = static_cast<uInt>(in_len);

    if (inflateInit2(&s, 15 + 32) != Z_OK) return false;

    int ret;
    do {
        if (s.total_out >= out.size()) out.resize(out.size() * 2);
        s.next_out = out.data() + s.total_out;
        s.avail_out = static_cast<uInt>(out.size() - s.total_out);
        ret = inflate(&s, Z_FINISH);
    } while (ret == Z_OK);

    inflateEnd(&s);
    if (ret != Z_STREAM_END) return false;
    out.resize(s.total_out);
    return true;
}

// ============================================================
// PBF Block reader
// ============================================================

struct PBFBlock { std::string type; std::vector<uint8_t> data; };

bool read_pbf_block(std::FILE* f, PBFBlock& block) {
    uint8_t len_buf[4];
    if (std::fread(len_buf, 1, 4, f) != 4) return false;
    uint32_t header_len = (static_cast<uint32_t>(len_buf[0]) << 24) |
                          (static_cast<uint32_t>(len_buf[1]) << 16) |
                          (static_cast<uint32_t>(len_buf[2]) << 8)  |
                          static_cast<uint32_t>(len_buf[3]);

    std::vector<uint8_t> header_data(header_len);
    if (std::fread(header_data.data(), 1, header_len, f) != header_len) return false;

    const uint8_t* hp = header_data.data();
    const uint8_t* he = hp + header_len;
    block.type.clear();
    uint32_t blob_size = 0;

    while (hp < he) {
        uint32_t fn; int wt;
        if (!read_tag(hp, he, fn, wt)) break;
        if (fn == 1 && wt == 2) {
            const uint8_t* sd; size_t sl;
            if (read_length_delimited(hp, he, sd, sl))
                block.type.assign(reinterpret_cast<const char*>(sd), sl);
        } else if (fn == 3 && wt == 0) {
            uint64_t v; if (read_varint(hp, he, v)) blob_size = v;
        } else { if (!skip_field(hp, he, wt)) return false; }
    }

    std::vector<uint8_t> blob_data(blob_size);
    if (std::fread(blob_data.data(), 1, blob_size, f) != blob_size) return false;

    const uint8_t* bp = blob_data.data();
    const uint8_t* be = bp + blob_size;
    const uint8_t* raw = nullptr; size_t raw_len = 0;
    const uint8_t* zlib = nullptr; size_t zlib_len = 0;
    int32_t raw_size = -1;

    while (bp < be) {
        uint32_t fn; int wt;
        if (!read_tag(bp, be, fn, wt)) break;
        if (fn == 1 && wt == 2) read_length_delimited(bp, be, raw, raw_len);
        else if (fn == 2 && wt == 0) { uint64_t v; if (read_varint(bp, be, v)) raw_size = static_cast<int32_t>(v); }
        else if (fn == 3 && wt == 2) read_length_delimited(bp, be, zlib, zlib_len);
        else { if (!skip_field(bp, be, wt)) return false; }
    }

    if (zlib) {
        size_t expected = raw_size > 0 ? static_cast<size_t>(raw_size) : zlib_len * 4;
        if (!zlib_decompress(zlib, zlib_len, block.data, expected)) return false;
    } else if (raw) {
        block.data.assign(raw, raw + raw_len);
    } else {
        return false;
    }
    return true;
}

// ============================================================
// PrimitiveBlock parser
// ============================================================

struct PBFParseState {
    OSMData& data;
    std::vector<std::string> strtab;
    int granularity = 100;
    int64_t lat_offset = 0;
    int64_t lon_offset = 0;
    uint64_t nodes_parsed = 0;
    uint64_t ways_parsed = 0;
};

bool parse_primitive_block(const std::vector<uint8_t>& buf, PBFParseState& st) {
    const uint8_t* p = buf.data();
    const uint8_t* end = p + buf.size();

    st.strtab.clear();
    st.granularity = 100;
    st.lat_offset = 0;
    st.lon_offset = 0;

    const uint8_t* strtab_data = nullptr; size_t strtab_len = 0;
    std::vector<const uint8_t*> groups;
    std::vector<size_t> group_lens;

    while (p < end) {
        uint32_t fn; int wt;
        if (!read_tag(p, end, fn, wt)) break;
        if (fn == 1 && wt == 2) {
            // StringTable: one blob containing varint-length-prefixed strings
            read_length_delimited(p, end, strtab_data, strtab_len);
        } else if (fn == 2 && wt == 2) {
            const uint8_t* gd; size_t gl;
            if (read_length_delimited(p, end, gd, gl)) {
                groups.push_back(gd);
                group_lens.push_back(gl);
            }
        } else if (fn == 17 && wt == 0) { uint64_t v; if (read_varint(p, end, v)) st.granularity = static_cast<int>(v); }
        else if (fn == 19 && wt == 0) { uint64_t v; if (read_varint(p, end, v)) st.lat_offset = static_cast<int64_t>(v); }
        else if (fn == 20 && wt == 0) { uint64_t v; if (read_varint(p, end, v)) st.lon_offset = static_cast<int64_t>(v); }
        else { if (!skip_field(p, end, wt)) return false; }
    }

    // Parse the string table (nested protobuf message: repeated field 1 strings)
    if (strtab_data) {
        const uint8_t* sp = strtab_data, *se = sp + strtab_len;
        while (sp < se) {
            uint32_t sfn; int swt;
            if (!read_tag(sp, se, sfn, swt)) break;
            if (sfn == 1 && swt == 2) { // each string is field 1, wire type 2
                const uint8_t* sd; size_t sl;
                if (read_length_delimited(sp, se, sd, sl))
                    st.strtab.emplace_back(reinterpret_cast<const char*>(sd), sl);
            } else {
                if (!skip_field(sp, se, swt)) break;
            }
        }
    }

    auto latlon_to_double = [&](int64_t raw) -> double {
        return (static_cast<double>(raw) * st.granularity) / 1e9;
    };

    // Parse each PrimitiveGroup
    for (size_t gi = 0; gi < groups.size(); ++gi) {
        const uint8_t* gp = groups[gi];
        const uint8_t* ge = gp + group_lens[gi];

        const uint8_t* dense_data = nullptr; size_t dense_len = 0;

        while (gp < ge) {
            uint32_t fn; int wt;
            if (!read_tag(gp, ge, fn, wt)) break;

            if (fn == 2 && wt == 2) { // DenseNodes
                read_length_delimited(gp, ge, dense_data, dense_len);
            } else if (fn == 3 && wt == 2) { // Way
                const uint8_t* wd; size_t wl;
                if (!read_length_delimited(gp, ge, wd, wl)) continue;

                const uint8_t* wp = wd, *we = wd + wl;
                int64_t way_id = 0;
                std::vector<uint32_t> way_keys, way_vals;
                const uint8_t* ref_data = nullptr; size_t ref_len = 0;

                while (wp < we) {
                    uint32_t wfn; int wwt;
                    if (!read_tag(wp, we, wfn, wwt)) break;
                    if (wfn == 1 && wwt == 0) { uint64_t v; if (read_varint(wp, we, v)) way_id = static_cast<int64_t>(v); }
                    else if (wfn == 2) { // keys — packed (wt=2) or non-packed (wt=0)
                        if (wwt == 2) {
                            const uint8_t* kd; size_t kl;
                            if (read_length_delimited(wp, we, kd, kl)) {
                                const uint8_t* kp = kd, *ke = kp + kl;
                                while (kp < ke) { uint64_t v; if (!read_varint(kp, ke, v)) break; way_keys.push_back(static_cast<uint32_t>(v)); }
                            }
                        } else if (wwt == 0) {
                            uint64_t v; if (read_varint(wp, we, v)) way_keys.push_back(static_cast<uint32_t>(v));
                        }
                    }
                    else if (wfn == 3) { // vals
                        if (wwt == 2) {
                            const uint8_t* vd; size_t vl;
                            if (read_length_delimited(wp, we, vd, vl)) {
                                const uint8_t* vp = vd, *ve = vp + vl;
                                while (vp < ve) { uint64_t v; if (!read_varint(vp, ve, v)) break; way_vals.push_back(static_cast<uint32_t>(v)); }
                            }
                        } else if (wwt == 0) {
                            uint64_t v; if (read_varint(wp, we, v)) way_vals.push_back(static_cast<uint32_t>(v));
                        }
                    }
                    else if (wfn == 8 && wwt == 2) read_length_delimited(wp, we, ref_data, ref_len);
                    else { if (!skip_field(wp, we, wwt)) break; }
                }
                if (way_id == 0) continue;

                Way way{};
                way.id = way_id;

                // Pair keys and vals
                size_t nkv = std::min(way_keys.size(), way_vals.size());
                for (size_t j = 0; j < nkv; ++j) {
                    uint32_t k = way_keys[j], v = way_vals[j];
                    if (k < st.strtab.size() && v < st.strtab.size())
                        way.tags[st.strtab[k]] = st.strtab[v];
                }

                if (ref_data) {
                    const uint8_t* rp = ref_data, *re = rp + ref_len;
                    int64_t prev = 0;
                    while (rp < re) {
                        uint64_t v;
                        if (!read_varint(rp, re, v)) break;
                        prev += zigzag64(v);
                        way.node_refs.push_back(prev);
                    }
                }

                st.data.ways[way_id] = std::move(way);
                st.ways_parsed++;
            } else if (fn == 4 && wt == 2) { // Relation (skip)
                const uint8_t* rd; size_t rl;
                read_length_delimited(gp, ge, rd, rl);
            } else {
                if (!skip_field(gp, ge, wt)) break;
            }
        }

        // Parse DenseNodes
        if (dense_data) {
            const uint8_t* dp = dense_data, *de = dp + dense_len;
            std::vector<int64_t> ids, lats, lons;
            std::vector<int32_t> keys_vals;

            while (dp < de) {
                uint32_t dfn; int dwt;
                if (!read_tag(dp, de, dfn, dwt)) break;
                if (dfn == 1 && dwt == 2) {
                    const uint8_t* sd; size_t sl;
                    if (read_length_delimited(dp, de, sd, sl)) {
                        const uint8_t* sp = sd;
                        uint64_t prev = 0;
                        while (sp < sd + sl) {
                            uint64_t v;
                            if (!read_varint(sp, sd + sl, v)) break;
                            prev += zigzag64(v);
                            ids.push_back(static_cast<int64_t>(prev));
                        }
                    }
                } else if (dfn == 8 && dwt == 2) {
                    const uint8_t* sd; size_t sl;
                    if (read_length_delimited(dp, de, sd, sl)) {
                        const uint8_t* sp = sd;
                        int64_t prev = 0;
                        while (sp < sd + sl) {
                            uint64_t v;
                            if (!read_varint(sp, sd + sl, v)) break;
                            prev += zigzag64(v);
                            lats.push_back(prev);
                        }
                    }
                } else if (dfn == 9 && dwt == 2) {
                    const uint8_t* sd; size_t sl;
                    if (read_length_delimited(dp, de, sd, sl)) {
                        const uint8_t* sp = sd;
                        int64_t prev = 0;
                        while (sp < sd + sl) {
                            uint64_t v;
                            if (!read_varint(sp, sd + sl, v)) break;
                            prev += zigzag64(v);
                            lons.push_back(prev);
                        }
                    }
                } else if (dfn == 10 && dwt == 2) {
                    const uint8_t* sd; size_t sl;
                    if (read_length_delimited(dp, de, sd, sl)) {
                        const uint8_t* sp = sd;
                        while (sp < sd + sl) {
                            uint64_t v;
                            if (!read_varint(sp, sd + sl, v)) break;
                            keys_vals.push_back(static_cast<int32_t>(v));
                        }
                    }
                } else { if (!skip_field(dp, de, dwt)) break; }
            }

            size_t n = ids.size();
            if (n > 0 && lats.size() == n && lons.size() == n) {
                size_t kv_idx = 0;
                for (size_t i = 0; i < n; ++i) {
                    Node node{};
                    node.id = ids[i];
                    node.lat = latlon_to_double(st.lat_offset + lats[i]);
                    node.lon = latlon_to_double(st.lon_offset + lons[i]);
                    while (kv_idx < keys_vals.size()) {
                        if (keys_vals[kv_idx] == 0) { kv_idx++; break; }
                        kv_idx++; if (kv_idx >= keys_vals.size()) break;
                        kv_idx++;
                    }
                    st.data.nodes[node.id] = node;
                    st.nodes_parsed++;
                }
            }
        }
    }
    return true;
}

} // namespace

// ============================================================
// Public API
// ============================================================

OSMData parse_osm_pbf(const std::string& filepath) {
    std::FILE* f = std::fopen(filepath.c_str(), "rb");
    if (!f) throw std::runtime_error("Failed to open: " + filepath);

    OSMData data;
    PBFParseState state{data, {}};

    PBFBlock block;
    bool first = true;
    while (read_pbf_block(f, block)) {
        if (first) {
            if (block.type != "OSMHeader")
                throw std::runtime_error("Expected OSMHeader, got: " + block.type);
            first = false;
            continue;
        }
        if (block.type != "OSMData") continue;
        parse_primitive_block(block.data, state);

        if (state.nodes_parsed % 10000000 == 0 && state.nodes_parsed > 0) {
            std::fprintf(stderr, "  parsed %lu nodes, %lu ways...\n",
                state.nodes_parsed, state.ways_parsed);
        }
    }

    std::fclose(f);
    std::fprintf(stderr, "  total: %lu nodes, %lu ways\n", state.nodes_parsed, state.ways_parsed);
    return data;
}
