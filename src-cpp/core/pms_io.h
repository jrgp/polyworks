#pragma once
/*
 * pms_io.h — PMS file read / write / compile.
 *
 * Three public functions:
 *   loadPmsFile  — read .pms/.pfb from disk → PmsData
 *   savePmsFile  — write PolyWorks-native .pms (MapRandomID=-1)
 *   compilePms   — write compiled game-ready .pms (sector table, centred coords)
 *
 * Two conversion helpers:
 *   pmsDataToDoc / docToPmsData — bridge between PmsData and MapDocument
 *
 * Two in-memory helpers (used for undo snapshots):
 *   pmsDataToBytes / pmsBytesToData
 */

#include "pms_types.h"
#include "map_document.h"

#include <string>
#include <vector>

/* ---- PMS data container (mirrors on-disk structure exactly) ------------ */

struct SectorCell {
    int              polyCount = 0;
    std::vector<int> polyIndex;  /* 0-based indices */
};

struct PmsData {
    int32_t  version = PMS_VERSION;
    PmsOptions options{};

    std::vector<PmsPolyEntry>    polys;
    int32_t  sectorDiv = 1;
    SectorCell sectors[SECTOR_CELLS][SECTOR_CELLS]{};

    std::vector<PmsProp>         props;
    std::vector<PmsSceneryName>  sceneryNames;
    std::vector<PmsCollider>     colliders;
    std::vector<PmsSpawnPoint>   spawns;
    std::vector<PmsWaypoint>     waypoints;
    std::vector<PmsLight>        lights;
    std::vector<PmsSketchLine>   sketch;
};

enum class PmsLoadResult { OK, FileNotFound, VersionMismatch, Truncated, Corrupt };

/* ---- File I/O ---------------------------------------------------------- */

PmsLoadResult loadPmsFile(const std::string& path, PmsData& out,
                          std::string& err);

bool savePmsFile(const std::string& path, const PmsData& data,
                 std::string& err);

bool compilePms(const std::string& path, const PmsData& data,
                std::string& err);

/* ---- MapDocument ↔ PmsData --------------------------------------------- */

void docToPmsData(const MapDocument& doc, PmsData& out);
void pmsDataToDoc(const PmsData& data, MapDocument& doc);

/* ---- In-memory helpers for undo snapshots ------------------------------ */

bool pmsDataToBytes(const PmsData& data, std::vector<uint8_t>& out,
                    std::string& err);

bool pmsBytesToData(const std::vector<uint8_t>& bytes, PmsData& out,
                    std::string& err);

/* ---- Prefab save / load (VB6-compatible .pwf format) ------------------- */
/* Saves all currently-selected entities from doc to a .pwf prefab file.
   Returns true on success; sets err on failure. */
bool savePrefab(const std::string& path, const MapDocument& doc,
                std::string& err);

/* Loads a .pwf prefab file and appends its contents to doc (selected).
   Returns true on success; sets err on failure. */
bool loadPrefab(const std::string& path, MapDocument& doc,
                std::string& err);
