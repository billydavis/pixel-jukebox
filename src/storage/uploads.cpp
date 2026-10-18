#include "storage/uploads.h"

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "gif_pad.h"
#include "storage/card.h"
#include "upload_rules.h"

static const char *UPLOAD_DIR = CARD_ROOT "/uploads";
static const char *PART_FILE = CARD_ROOT "/uploads/upload.part";
static const uint64_t KEEP_FREE_BYTES = 8ULL << 20;

static bool ready = false;
static std::vector<UploadInfo> list; // oldest first
static uint32_t nextId = 1;

static std::vector<uint32_t> order; // the shuffled play order
static size_t orderPosition = 0;

// The upload being received.
static FILE *part = nullptr;
static size_t expected = 0;
static size_t received = 0;
static String partName;
static uint8_t header[10];
static int partWidth = 0, partHeight = 0;

namespace {
struct Guard {
  CardLock card;
};
} // namespace

static String pathFor(const UploadInfo &info) {
  return String(UPLOAD_DIR) + "/" + uploads::fileName(info.id, info.width, info.height, info.name.c_str()).c_str();
}

void uploadsBegin() {
  Guard guard;
  if (!cardReady()) {
    return;
  }
  mkdir(UPLOAD_DIR, 0777);
  remove(PART_FILE); // left by an upload cut short
  DIR *dir = opendir(UPLOAD_DIR);
  if (!dir) {
    return;
  }
  while (struct dirent *item = readdir(dir)) {
    uint32_t id;
    int width, height;
    std::string name;
    if (!uploads::parseFileName(item->d_name, id, width, height, name)) {
      continue;
    }
    struct stat info;
    String path = String(UPLOAD_DIR) + "/" + item->d_name;
    uint32_t bytes = stat(path.c_str(), &info) == 0 ? info.st_size : 0; // a small folder: cheap
    list.push_back({id, name.c_str(), (uint16_t)width, (uint16_t)height, bytes});
    nextId = max(nextId, id + 1);
  }
  closedir(dir);
  std::sort(list.begin(), list.end(), [](const UploadInfo &a, const UploadInfo &b) { return a.id < b.id; });
  ready = true;
  Serial.printf("Uploads: %u\n", (unsigned)list.size());
}

bool uploadsAvailable() {
  Guard guard;
  return ready;
}

std::vector<UploadInfo> uploadsList() {
  Guard guard;
  std::vector<UploadInfo> newestFirst(list.rbegin(), list.rend());
  return newestFirst;
}

bool uploadsFind(uint32_t id, UploadInfo &info) {
  Guard guard;
  for (const UploadInfo &each : list) {
    if (each.id == id) {
      info = each;
      return true;
    }
  }
  return false;
}

bool uploadsRead(uint32_t id, uint8_t *&data, size_t &size) {
  Guard guard;
  UploadInfo info;
  if (!uploadsFind(id, info)) {
    return false;
  }
  FILE *file = fopen(pathFor(info).c_str(), "rb");
  if (!file) {
    return false;
  }
  data = nullptr;
  size = 0;
  if (fseek(file, 0, SEEK_END) == 0) {
    long length = ftell(file);
    if (length > 6 && (size_t)length <= uploads::MAX_BYTES) {
      size = length;
      data = (uint8_t *)ps_malloc(size + GIF_PAD_BYTES);
      if (data && (fseek(file, 0, SEEK_SET) != 0 || fread(data, 1, size, file) != size)) {
        free(data);
        data = nullptr;
      }
    }
  }
  fclose(file);
  return data != nullptr;
}

bool uploadsNext(uint32_t &id) {
  Guard guard;
  if (list.empty()) {
    return false;
  }
  // A new shuffle after each full pass, or when the list has changed size.
  if (orderPosition >= order.size() || order.size() != list.size()) {
    order.clear();
    for (const UploadInfo &each : list) {
      order.push_back(each.id);
    }
    for (size_t i = order.size(); i > 1; i--) {
      std::swap(order[i - 1], order[esp_random() % i]);
    }
    orderPosition = 0;
  }
  // One removed since the shuffle is skipped.
  while (orderPosition < order.size()) {
    UploadInfo info;
    id = order[orderPosition++];
    if (uploadsFind(id, info)) {
      return true;
    }
  }
  return false;
}

bool uploadsRemove(uint32_t id) {
  Guard guard;
  for (size_t i = 0; i < list.size(); i++) {
    if (list[i].id == id) {
      remove(pathFor(list[i]).c_str());
      list.erase(list.begin() + i);
      return true;
    }
  }
  return false;
}

bool uploadsStart(size_t length, const String &name, String &error) {
  Guard guard;
  if (!ready) {
    error = "There is no card in the panel";
    return false;
  }
  if (part) {
    error = "Another upload is in progress";
    return false;
  }
  if ((int)list.size() >= uploads::MAX_UPLOADS) {
    error = "There are already 200 uploads; remove some first";
    return false;
  }
  if (length > uploads::MAX_BYTES) {
    error = "The file is over 512 KB";
    return false;
  }
  JsonDocument card;
  cardReport(card.to<JsonObject>());
  if (card["freeBytes"].as<uint64_t>() < length + KEEP_FREE_BYTES) {
    error = "The card is full";
    return false;
  }
  part = fopen(PART_FILE, "wb");
  if (!part) {
    error = "Could not write to the card";
    return false;
  }
  expected = length;
  received = 0;
  partName = name;
  partWidth = partHeight = 0;
  return true;
}

bool uploadsWrite(const uint8_t *data, size_t length, String &error) {
  Guard guard;
  if (!part) {
    return false;
  }
  // The header arrives first: the file is checked before more is kept.
  for (size_t i = 0; i < length && received + i < sizeof(header); i++) {
    header[received + i] = data[i];
  }
  bool headerDone = received < sizeof(header) && received + length >= sizeof(header);
  received += length;
  if (headerDone) {
    if (!uploads::gifSize(header, sizeof(header), partWidth, partHeight)) {
      error = "That is not a GIF";
      return false;
    }
    std::string problem = uploads::sizeProblem(partWidth, partHeight, expected);
    if (!problem.empty()) {
      error = String("It cannot be shown: ") + problem.c_str();
      return false;
    }
  }
  if (fwrite(data, 1, length, part) != length) {
    error = "Could not write to the card";
    return false;
  }
  return true;
}

bool uploadsFinish(UploadInfo &info, String &error) {
  Guard guard;
  if (!part) {
    error = "No upload in progress";
    return false;
  }
  bool written = fclose(part) == 0;
  part = nullptr;
  if (!written || received != expected || partWidth == 0) {
    remove(PART_FILE);
    error = received < sizeof(header) ? "That is not a GIF" : "The upload did not complete";
    return false;
  }
  info = {nextId, uploads::tidyName(partName.c_str()).c_str(), (uint16_t)partWidth, (uint16_t)partHeight,
          (uint32_t)received};
  if (rename(PART_FILE, pathFor(info).c_str()) != 0) {
    remove(PART_FILE);
    error = "Could not save it on the card";
    return false;
  }
  nextId++;
  list.push_back(info);
  Serial.printf("Uploads: added %u, %s, %ux%u\n", (unsigned)info.id, info.name.c_str(), info.width, info.height);
  return true;
}

void uploadsAbort() {
  Guard guard;
  if (part) {
    fclose(part);
    part = nullptr;
  }
  remove(PART_FILE);
}

void uploadsReport(JsonObject out) {
  Guard guard;
  out["available"] = ready;
  out["count"] = list.size();
  out["max"] = uploads::MAX_UPLOADS;
  uint64_t bytes = 0;
  for (const UploadInfo &each : list) {
    bytes += each.bytes;
  }
  out["bytes"] = bytes;
}
