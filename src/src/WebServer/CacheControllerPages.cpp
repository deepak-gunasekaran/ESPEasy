#include "../WebServer/CacheControllerPages.h"

#ifdef USES_C016

# include "../WebServer/ESPEasy_WebServer.h"
# include "../WebServer/AccessControl.h"
# include "../WebServer/HTML_wrappers.h"
# include "../WebServer/JSON.h"
# include "../CustomBuild/ESPEasyLimits.h"
# include "../DataStructs/DeviceStruct.h"
# include "../DataTypes/TaskIndex.h"
# include "../Globals/C016_ControllerCache.h"
# include "../Globals/Cache.h"
# include "../Globals/ESPEasy_time.h"
# include "../Globals/Settings.h"
# include "../Helpers/ESPEasy_math.h"
# include "../Helpers/ESPEasy_Storage.h"
# include "../Helpers/ESPEasy_time_calc.h"
# include "../Helpers/Misc.h"


// ********************************************************************************
// URLs needed for C016_CacheController
// to help dump the content of the binary log files
// ********************************************************************************
void handle_dumpcache() {
  if (!isLoggedIn()) { return; }

  // Filters/export settings
  char separator     = ';';
  bool joinTimestamp = false;
  bool onlySetTasks  = false;


  if (hasArg(F("separator"))) {
    String sep = webArg(F("separator"));

    if (isWrappedWithQuotes(sep)) {
      removeChar(sep, sep[0]);
    }

    if (sep.equalsIgnoreCase(F("Tab"))) { separator = '\t'; }
    else if (sep.equalsIgnoreCase(F("Comma"))) { separator = ','; }
    else if (sep.equalsIgnoreCase(F("Semicolon"))) { separator = ';'; }
  }

  if (hasArg(F("jointimestamp"))) {
    joinTimestamp = true;
  }

  if (hasArg(F("onlysettasks"))) {
    onlySetTasks = true;
  }


  // Allocate a String per taskvalue instead of per task
  // This way the small strings will hardly ever need heap allocation.
  constexpr size_t nrTaskValues = VARS_PER_TASK * TASKS_MAX;
  String  csv_values[nrTaskValues];
  uint8_t nrDecimals[nrTaskValues] = { 0 };
  bool    includeTask[TASKS_MAX]   = { 0 };
  {
    // Initialize arrays
    const String sep_zero = String(separator) + '0';

    for (size_t i = 0; i < nrTaskValues; ++i) {
      csv_values[i] = sep_zero;
      nrDecimals[i] = Cache.getTaskDeviceValueDecimals(i / VARS_PER_TASK, i % VARS_PER_TASK);
    }

    for (size_t task = 0; validTaskIndex(task); ++task) {
      includeTask[task] = onlySetTasks ? validPluginID(Settings.TaskDeviceNumber[task]) : true;
    }
  }


  // First backup the peek file positions.
  int peekFileNr;
  const int peekFilePos = ControllerCache.getPeekFilePos(peekFileNr);

  // Set peek file position to first entry:
  ControllerCache.setPeekFilePos(0, 0);

  C016_flush();

  {
    // Send HTTP headers to directly save the dump as a CSV file
    String str =  F("attachment; filename=cachedump_");
    str += Settings.Name;
    str += F("_U");
    str += Settings.Unit;

    if (node_time.systemTimePresent())
    {
      str += '_';
      str += node_time.getDateTimeString('\0', '\0', '\0');
    }
    str += F(".csv");

    sendHeader(F("Content-Disposition"), str);
    TXBuffer.startStream(F("application/octet-stream"), F("*"), 200);
  }

  {
    // CSV header
    String header(F("UNIX timestamp;UTC timestamp"));

    if (joinTimestamp) {
      // Add column with nr of joined samples
      header += F(";nrJoinedSamples");
    } else {
      // Does not make sense to have taskindex and plugin ID
      // in a table where separate samples may have been combined.
      header += F(";taskindex;plugin ID");
    }

    if (separator != ';') { header.replace(';', separator); }
    addHtml(header);

    for (taskIndex_t i = 0; i < TASKS_MAX; ++i) {
      if (includeTask[i]) {
        for (int j = 0; j < VARS_PER_TASK; ++j) {
          addHtml(separator);
          addHtml(getTaskDeviceName(i));
          addHtml('#');
          addHtml(getTaskValueName(i, j));
        }
      }
    }
    addHtml('\r', '\n');
  }


  // Fetch samples from Cache Controller bin files.
  C016_binary_element element;

  uint32_t lastTimestamp = 0;
  int csv_values_left    = 0;

  while (C016_getTaskSample(element)) {
    if (!joinTimestamp || (lastTimestamp != static_cast<uint32_t>(element._timestamp))) {
      // Flush the collected CSV values
      if (csv_values_left > 0) {
        if (joinTimestamp) {
          // Add column with nr of joined samples
          addHtml(';');
          addHtmlInt(csv_values_left);
        }

        for (size_t i = 0; i < nrTaskValues; ++i) {
          if (includeTask[i / VARS_PER_TASK]) {
            addHtml(csv_values[i]);
          }
        }
        addHtml('\r', '\n');
        csv_values_left = 0;
      }

      // Start writing a new line in the CSV file
      // Begin with the non taskvalues
      addHtmlInt(static_cast<uint32_t>(element._timestamp));
      addHtml(separator);
      struct tm ts;
      breakTime(element._timestamp, ts);
      addHtml(formatDateTimeString(ts));

      if (!joinTimestamp) {
        addHtml(separator);
        addHtmlInt(element.TaskIndex);
        addHtml(separator);
        addHtmlInt(element.pluginID);
      }

      lastTimestamp = static_cast<uint32_t>(element._timestamp);
    }
    ++csv_values_left;

    // Collect the task values for this row in the CSV
    size_t valindex = element.TaskIndex * VARS_PER_TASK;

    for (size_t i = 0; i < VARS_PER_TASK; ++i) {
      csv_values[valindex] = separator;

      if (essentiallyZero(element.values[i])) {
        csv_values[valindex] += '0';
      } else {
        csv_values[valindex] += toString(element.values[i], static_cast<unsigned int>(nrDecimals[valindex]));
      }
      ++valindex;
    }
  }

  if (csv_values_left > 0) {
    if (joinTimestamp) {
      // Add column with nr of joined samples
      addHtml(';');
      addHtmlInt(csv_values_left);
    }

    for (size_t i = 0; i < nrTaskValues; ++i) {
      if (includeTask[i / VARS_PER_TASK]) {
        addHtml(csv_values[i]);
      }
    }
    addHtml('\r', '\n');
  }

  TXBuffer.endStream();

  // Restore peek file positions.
  ControllerCache.setPeekFilePos(peekFileNr, peekFilePos);
}

void handle_cache_json() {
  if (!isLoggedIn()) { return; }

  // Flush any data still in RTC memory to the cache files.
  C016_flush();

  TXBuffer.startJsonStream();
  addHtml(F("{\"columns\": ["));

  //     addHtml(F("UNIX timestamp;contr. idx;sensortype;taskindex;value count"));
  addHtml(to_json_value(F("UNIX timestamp")));
  addHtml(',');
  addHtml(to_json_value(F("UTC timestamp")));
  addHtml(',');
  addHtml(to_json_value(F("task index")));

  if (hasArg(F("pluginID"))) {
    addHtml(',');
    addHtml(to_json_value(F("plugin ID")));
  }

  for (taskIndex_t i = 0; i < TASKS_MAX; ++i) {
    for (int j = 0; j < VARS_PER_TASK; ++j) {
      String label = getTaskDeviceName(i);
      label += '#';
      label += getTaskValueName(i, j);
      addHtml(',');
      addHtml(to_json_value(label));
    }
  }
  addHtml(F("],\n"));
  addHtml(F("\"files\": ["));
  bool islast    = false;
  int  filenr    = 0;
  int  fileCount = 0;

  while (!islast) {
    const String currentFile = C016_getCacheFileName(filenr, islast);
    ++filenr;

    if (currentFile.length() > 0) {
      if (fileCount != 0) {
        addHtml(',');
      }
      addHtml(to_json_value(currentFile));
      ++fileCount;
    }
  }
  addHtml(F("],\n"));
  addHtml(F("\"pluginID\": ["));

  for (taskIndex_t taskIndex = 0; validTaskIndex(taskIndex); ++taskIndex) {
    if (taskIndex != 0) {
      addHtml(',');
    }
    addHtmlInt(getPluginID_from_TaskIndex(taskIndex));
  }
  addHtml(F("],\n"));
  stream_next_json_object_value(F("separator"), F(";"));
  stream_last_json_object_value(F("nrfiles"), fileCount);
  addHtml('\n');
  TXBuffer.endStream();
}

void handle_cache_csv() {
  if (!isLoggedIn()) { return; }
}

#endif // ifdef USES_C016
