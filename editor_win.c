#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>

#include "editor_win.h"

#ifndef ES_DISABLENOSCROLL
#define ES_DISABLENOSCROLL 0x2000L
#endif

#ifndef EM_REDO
#define EM_REDO 0x0454
#endif

#ifndef EM_CANREDO
#define EM_CANREDO 0x0455
#endif

#define WINDOW_CLASS_NAME "UnnamedLangEditorWindow"
#define WINDOW_TITLE "Namaste DSL Editor"

#define OUTPUT_BAR_HEIGHT 24
#define MIN_CODE_EDITOR_HEIGHT 140
#define MIN_OUTPUT_HEIGHT 100
#define MIN_WINDOW_WIDTH 720
#define MIN_WINDOW_HEIGHT 520
#define PROCESS_LOG_BUFFER_SIZE 16384
#define COMBINED_LOG_BUFFER_SIZE 32768
#define WM_SYNC_SCROLLBARS (WM_APP + 1)
#define CODE_EDIT_ORIGINAL_PROC_PROP "UnnamedLangCodeEditOriginalProc"

#define ID_SOURCE_PATH_EDIT 1001
#define ID_OPEN_BUTTON 1002
#define ID_SAVE_AS_BUTTON 1003
#define ID_COMPILE_BUTTON 1004
#define ID_PATH_EDIT 1005
#define ID_CODE_EDIT 1006
#define ID_LOG_EDIT 1007
#define ID_SOURCE_LABEL 1008
#define ID_PATH_LABEL 1009
#define ID_CODE_VSCROLL 1010
#define ID_CODE_HSCROLL 1011
#define ID_LOG_VSCROLL 1012
#define ID_LOG_HSCROLL 1013

typedef struct {
  HWND window_handle;
  HWND source_path_edit;
  HWND open_button;
  HWND save_as_button;
  HWND compile_button;
  HWND path_edit;
  HWND code_edit;
  HWND log_edit;
  HWND code_vscroll;
  HWND code_hscroll;
  HWND log_vscroll;
  HWND log_hscroll;
  HFONT ui_font;
  HFONT code_font;
  RECT output_bar_rect;
  int output_height;
  int is_dragging_output;
  int drag_offset_y;
  int code_horizontal_scroll;
  int log_horizontal_scroll;
  char current_source_path[MAX_PATH];
  char executable_path[MAX_PATH];
  char project_directory[MAX_PATH];
} EditorState;

static int load_source_from_path(EditorState *state, const char *source_path, int show_errors);
static void set_default_source_path(EditorState *state, const char *source_path, const char *source_text);
static void request_scrollbar_sync(EditorState *state);
static void focus_code_editor(EditorState *state);
static LRESULT CALLBACK code_edit_subclass_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

static int key_matches(WPARAM w_param, int upper_key){
  int key = (int)w_param;
  return key == upper_key || key == (upper_key + ('a' - 'A'));
}

static LRESULT CALLBACK code_edit_subclass_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param){
  WNDPROC default_proc = (WNDPROC)GetPropA(window_handle, CODE_EDIT_ORIGINAL_PROC_PROP);

  if(default_proc == NULL){
    return DefWindowProcA(window_handle, message, w_param, l_param);
  }

  if(message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0){
    if(key_matches(w_param, 'A')){
      SendMessageA(window_handle, EM_SETSEL, 0, -1);
      return 0;
    }

    if(key_matches(w_param, 'Z')){
      int can_redo = (int)SendMessageA(window_handle, EM_CANREDO, 0, 0);
      if((GetKeyState(VK_SHIFT) & 0x8000) != 0 && can_redo){
        SendMessageA(window_handle, EM_REDO, 0, 0);
      } else if((int)SendMessageA(window_handle, EM_CANUNDO, 0, 0)){
        SendMessageA(window_handle, WM_UNDO, 0, 0);
      }
      return 0;
    }

    if(key_matches(w_param, 'Y') && (int)SendMessageA(window_handle, EM_CANREDO, 0, 0)){
      SendMessageA(window_handle, EM_REDO, 0, 0);
      return 0;
    }
  }

  return CallWindowProcA(default_proc, window_handle, message, w_param, l_param);
}

static const char *find_last_path_separator(const char *path){
  const char *last_backslash = strrchr(path, '\\');
  const char *last_forward_slash = strrchr(path, '/');

  if(last_backslash == NULL){
    return last_forward_slash;
  }

  if(last_forward_slash == NULL){
    return last_backslash;
  }

  if(last_backslash > last_forward_slash){
    return last_backslash;
  }

  return last_forward_slash;
}

static void trim_in_place(char *text){
  size_t length;
  size_t start_index = 0;

  while(text[start_index] == ' ' || text[start_index] == '\t' || text[start_index] == '\r' || text[start_index] == '\n'){
    start_index++;
  }

  if(start_index > 0){
    memmove(text, text + start_index, strlen(text + start_index) + 1);
  }

  length = strlen(text);
  while(length > 0){
    char current = text[length - 1];
    if(current != ' ' && current != '\t' && current != '\r' && current != '\n'){
      break;
    }
    text[length - 1] = '\0';
    length--;
  }
}

static void copy_string(char *destination, size_t destination_size, const char *source){
  if(destination_size == 0){
    return;
  }

  snprintf(destination, destination_size, "%s", source);
}

static int is_absolute_path(const char *path){
  if(path[0] == '\0'){
    return 0;
  }

  if(strlen(path) >= 2 && path[1] == ':'){
    return 1;
  }

  return path[0] == '\\' && path[1] == '\\';
}

static void get_directory_name(const char *path, char *directory, size_t directory_size){
  const char *last_separator = strrchr(path, '\\');

  if(last_separator == NULL){
    snprintf(directory, directory_size, ".");
    return;
  }

  {
    size_t length = (size_t)(last_separator - path);
    if(length >= directory_size){
      length = directory_size - 1;
    }
    memcpy(directory, path, length);
    directory[length] = '\0';
  }
}

static void join_paths(const char *base_path, const char *relative_path, char *result, size_t result_size){
  if(base_path[0] == '\0'){
    snprintf(result, result_size, "%s", relative_path);
    return;
  }

  if(base_path[strlen(base_path) - 1] == '\\'){
    snprintf(result, result_size, "%s%s", base_path, relative_path);
    return;
  }

  snprintf(result, result_size, "%s\\%s", base_path, relative_path);
}

static void resolve_source_path(EditorState *state, const char *user_path, char *resolved_path, size_t resolved_path_size){
  if(is_absolute_path(user_path)){
    snprintf(resolved_path, resolved_path_size, "%s", user_path);
    return;
  }

  join_paths(state->project_directory, user_path, resolved_path, resolved_path_size);
}

static int derive_path_with_extension(const char *path, const char *extension, char *result_path, size_t result_path_size){
  const char *last_separator = find_last_path_separator(path);
  const char *last_dot = strrchr(path, '.');
  size_t prefix_length = strlen(path);
  size_t extension_length = strlen(extension);

  if(path[0] == '\0'){
    result_path[0] = '\0';
    return 1;
  }

  if(last_dot != NULL && (last_separator == NULL || last_dot > last_separator)){
    prefix_length = (size_t)(last_dot - path);
  }

  if(prefix_length + extension_length >= result_path_size){
    return 0;
  }

  memcpy(result_path, path, prefix_length);
  memcpy(result_path + prefix_length, extension, extension_length + 1);
  return 1;
}

static char *read_text_file(const char *path){
  FILE *file = fopen(path, "rb");
  long file_size;
  char *buffer;

  if(file == NULL){
    return NULL;
  }

  if(fseek(file, 0, SEEK_END) != 0){
    fclose(file);
    return NULL;
  }

  file_size = ftell(file);
  if(file_size < 0){
    fclose(file);
    return NULL;
  }

  if(fseek(file, 0, SEEK_SET) != 0){
    fclose(file);
    return NULL;
  }

  buffer = malloc((size_t)file_size + 1);
  if(buffer == NULL){
    fclose(file);
    return NULL;
  }

  if(fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size){
    free(buffer);
    fclose(file);
    return NULL;
  }

  buffer[file_size] = '\0';
  fclose(file);
  return buffer;
}

static int write_text_file(const char *path, const char *contents, char *error_message, size_t error_message_size){
  FILE *file = fopen(path, "wb");

  if(file == NULL){
    snprintf(error_message, error_message_size, "Could not save %s", path);
    return 0;
  }

  if(fwrite(contents, 1, strlen(contents), file) != strlen(contents)){
    snprintf(error_message, error_message_size, "Could not write all content to %s", path);
    fclose(file);
    return 0;
  }

  fclose(file);
  return 1;
}

static char *convert_text_to_windows_newlines(const char *text){
  size_t source_index = 0;
  size_t result_length = 0;
  char *result = NULL;
  size_t result_index = 0;

  while(text[source_index] != '\0'){
    if(text[source_index] == '\r'){
      result_length += 2;
      if(text[source_index + 1] == '\n'){
        source_index++;
      }
    } else if(text[source_index] == '\n'){
      result_length += 2;
    } else {
      result_length += 1;
    }
    source_index++;
  }

  result = malloc(result_length + 1);
  if(result == NULL){
    return NULL;
  }

  source_index = 0;
  while(text[source_index] != '\0'){
    if(text[source_index] == '\r'){
      result[result_index++] = '\r';
      result[result_index++] = '\n';
      if(text[source_index + 1] == '\n'){
        source_index++;
      }
    } else if(text[source_index] == '\n'){
      result[result_index++] = '\r';
      result[result_index++] = '\n';
    } else {
      result[result_index++] = text[source_index];
    }
    source_index++;
  }

  result[result_index] = '\0';
  return result;
}

static char *convert_text_to_file_newlines(const char *text){
  size_t source_index = 0;
  size_t result_length = 0;
  char *result = NULL;
  size_t result_index = 0;

  while(text[source_index] != '\0'){
    if(text[source_index] == '\r'){
      result_length += 1;
      if(text[source_index + 1] == '\n'){
        source_index++;
      }
    } else {
      result_length += 1;
    }
    source_index++;
  }

  result = malloc(result_length + 1);
  if(result == NULL){
    return NULL;
  }

  source_index = 0;
  while(text[source_index] != '\0'){
    if(text[source_index] == '\r'){
      result[result_index++] = '\n';
      if(text[source_index + 1] == '\n'){
        source_index++;
      }
    } else {
      result[result_index++] = text[source_index];
    }
    source_index++;
  }

  result[result_index] = '\0';
  return result;
}

static void set_editor_text(EditorState *state, const char *text){
  char *normalized_text = convert_text_to_windows_newlines(text);

  state->code_horizontal_scroll = 0;

  if(normalized_text != NULL){
    SetWindowTextA(state->code_edit, normalized_text);
    free(normalized_text);
    InvalidateRect(state->code_edit, NULL, TRUE);
    UpdateWindow(state->code_edit);
    request_scrollbar_sync(state);
    return;
  }

  SetWindowTextA(state->code_edit, text);
  InvalidateRect(state->code_edit, NULL, TRUE);
  UpdateWindow(state->code_edit);
  request_scrollbar_sync(state);
}

static void set_path_text(EditorState *state, const char *text){
  SetWindowTextA(state->path_edit, text);
  SendMessageA(state->path_edit, EM_SETSEL, 0, 0);
}

static void request_scrollbar_sync(EditorState *state){
  if(state->window_handle != NULL){
    PostMessageA(state->window_handle, WM_SYNC_SCROLLBARS, 0, 0);
  }
}

static void focus_code_editor(EditorState *state){
  if(state->code_edit != NULL && IsWindow(state->code_edit)){
    SetFocus(state->code_edit);
  }
}

static void set_log_text(EditorState *state, const char *text){
  int text_length;
  char *normalized_text = convert_text_to_windows_newlines(text);

  state->log_horizontal_scroll = 0;

  if(normalized_text != NULL){
    SetWindowTextA(state->log_edit, normalized_text);
    free(normalized_text);
  } else {
    SetWindowTextA(state->log_edit, text);
  }

  text_length = GetWindowTextLengthA(state->log_edit);
  SendMessageA(state->log_edit, EM_SETSEL, (WPARAM)text_length, (LPARAM)text_length);
  SendMessageA(state->log_edit, EM_SCROLLCARET, 0, 0);
  request_scrollbar_sync(state);
}

static char *get_window_text_copy(HWND control){
  int text_length = GetWindowTextLengthA(control);
  char *buffer = malloc((size_t)text_length + 1);

  if(buffer == NULL){
    return NULL;
  }

  GetWindowTextA(control, buffer, text_length + 1);
  return buffer;
}

static void load_initial_source(EditorState *state){
  char source_path[MAX_PATH];
  char sample_path[MAX_PATH];
  char default_source_path[MAX_PATH];

  resolve_source_path(state, "examples\\namaste_demo.dsl", source_path, sizeof(source_path));
  if(load_source_from_path(state, source_path, 0)){
    return;
  }

  resolve_source_path(state, "examples\\hello.dsl", sample_path, sizeof(sample_path));
  if(load_source_from_path(state, sample_path, 0)){
    return;
  }

  join_paths(state->project_directory, "interactive.dsl", default_source_path, sizeof(default_source_path));
  set_default_source_path(
    state,
    default_source_path,
    "namaste() {\r\n"
    "  likho(\"NAMASTE DSL SHURU\");\r\n"
    "\r\n"
    "  ginti total = 21 + 21;\r\n"
    "  likho(total);\r\n"
    "\r\n"
    "  niklo(0);\r\n"
    "}\r\n"
  );
}

static int derive_output_path_from_source(const char *source_path, char *output_path, size_t output_path_size){
  return derive_path_with_extension(source_path, ".exe", output_path, output_path_size);
}

static int derive_source_file_name(const char *source_name, char *source_file_name, size_t source_file_name_size){
  return derive_path_with_extension(source_name, ".dsl", source_file_name, source_file_name_size);
}

static void append_text(char *buffer, size_t buffer_size, const char *text){
  size_t current_length = strlen(buffer);

  if(current_length >= buffer_size - 1){
    return;
  }

  snprintf(buffer + current_length, buffer_size - current_length, "%s", text);
}

static void append_format(char *buffer, size_t buffer_size, const char *format, ...){
  size_t current_length = strlen(buffer);
  va_list arguments;

  if(current_length >= buffer_size - 1){
    return;
  }

  va_start(arguments, format);
  vsnprintf(buffer + current_length, buffer_size - current_length, format, arguments);
  va_end(arguments);
}

static void set_source_name_from_path(EditorState *state, const char *path){
  const char *file_name = find_last_path_separator(path);
  const char *last_dot = NULL;
  char display_name[MAX_PATH];
  size_t name_length;

  if(file_name == NULL){
    file_name = path;
  } else {
    file_name++;
  }

  last_dot = strrchr(file_name, '.');
  if(last_dot != NULL){
    name_length = (size_t)(last_dot - file_name);
  } else {
    name_length = strlen(file_name);
  }

  if(name_length >= sizeof(display_name)){
    name_length = sizeof(display_name) - 1;
  }

  memcpy(display_name, file_name, name_length);
  display_name[name_length] = '\0';
  SetWindowTextA(state->source_path_edit, display_name);
}

static int derive_source_path_from_name(EditorState *state, const char *source_name, char *source_path, size_t source_path_size){
  char source_name_copy[MAX_PATH];
  char source_file_name[MAX_PATH];
  char base_directory[MAX_PATH];

  copy_string(source_name_copy, sizeof(source_name_copy), source_name);
  trim_in_place(source_name_copy);

  if(source_name_copy[0] == '\0'){
    source_path[0] = '\0';
    return 1;
  }

  if(!derive_source_file_name(source_name_copy, source_file_name, sizeof(source_file_name))){
    return 0;
  }

  if(is_absolute_path(source_file_name)){
    copy_string(source_path, source_path_size, source_file_name);
    return 1;
  }

  if(find_last_path_separator(source_file_name) != NULL){
    resolve_source_path(state, source_file_name, source_path, source_path_size);
    return 1;
  }

  if(state->current_source_path[0] != '\0'){
    get_directory_name(state->current_source_path, base_directory, sizeof(base_directory));
  } else {
    copy_string(base_directory, sizeof(base_directory), state->project_directory);
  }

  join_paths(base_directory, source_file_name, source_path, source_path_size);
  return 1;
}

static void sync_path_display(EditorState *state){
  char source_name[MAX_PATH];
  char source_path[MAX_PATH];

  GetWindowTextA(state->source_path_edit, source_name, sizeof(source_name));
  if(!derive_source_path_from_name(state, source_name, source_path, sizeof(source_path))){
    source_path[0] = '\0';
  }

  set_path_text(state, source_path);
}

static void set_current_source_path(EditorState *state, const char *source_path){
  copy_string(state->current_source_path, sizeof(state->current_source_path), source_path);
  set_source_name_from_path(state, source_path);
  set_path_text(state, source_path);
}

static int load_source_from_path(EditorState *state, const char *source_path, int show_errors){
  char *source_text = read_text_file(source_path);

  if(source_text == NULL){
    if(show_errors){
      char error_message[512];
      snprintf(error_message, sizeof(error_message), "Could not open %s", source_path);
      MessageBoxA(state->window_handle, error_message, WINDOW_TITLE, MB_ICONERROR | MB_OK);
    }
    return 0;
  }

  set_editor_text(state, source_text);
  free(source_text);
  set_current_source_path(state, source_path);
  focus_code_editor(state);
  return 1;
}

static void set_default_source_path(EditorState *state, const char *source_path, const char *source_text){
  set_editor_text(state, source_text);
  set_current_source_path(state, source_path);
  focus_code_editor(state);
}

static void get_default_dialog_source_path(EditorState *state, char *source_path, size_t source_path_size){
  char source_name[MAX_PATH];

  GetWindowTextA(state->source_path_edit, source_name, sizeof(source_name));
  if(!derive_source_path_from_name(state, source_name, source_path, source_path_size) || source_path[0] == '\0'){
    join_paths(state->project_directory, "interactive.dsl", source_path, source_path_size);
  }
}

static int run_process_capture_output(
  const char *application_path,
  const char *command_line_text,
  const char *working_directory,
  char *log_buffer,
  size_t log_buffer_size,
  DWORD *exit_code
){
  SECURITY_ATTRIBUTES security_attributes;
  STARTUPINFOA startup_info;
  PROCESS_INFORMATION process_information;
  HANDLE read_pipe = NULL;
  HANDLE write_pipe = NULL;
  DWORD bytes_read = 0;
  DWORD total_bytes_read = 0;
  BOOL process_created;
  char read_buffer[512];
  char command_line[2048];

  copy_string(command_line, sizeof(command_line), command_line_text);

  memset(&security_attributes, 0, sizeof(security_attributes));
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;

  if(!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0)){
    snprintf(log_buffer, log_buffer_size, "Could not create compiler output pipe.");
    return 0;
  }

  if(!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)){
    snprintf(log_buffer, log_buffer_size, "Could not prepare compiler output pipe.");
    CloseHandle(read_pipe);
    CloseHandle(write_pipe);
    return 0;
  }

  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput = write_pipe;
  startup_info.hStdError = write_pipe;

  memset(&process_information, 0, sizeof(process_information));

  process_created = CreateProcessA(
    application_path,
    command_line,
    NULL,
    NULL,
    TRUE,
    CREATE_NO_WINDOW,
    NULL,
    working_directory,
    &startup_info,
    &process_information
  );

  CloseHandle(write_pipe);

  if(!process_created){
    snprintf(log_buffer, log_buffer_size, "Could not start compiler process.");
    CloseHandle(read_pipe);
    return 0;
  }

  while(ReadFile(read_pipe, read_buffer, sizeof(read_buffer) - 1, &bytes_read, NULL) && bytes_read > 0){
    size_t remaining_space = 0;
    read_buffer[bytes_read] = '\0';

    if(total_bytes_read >= log_buffer_size - 1){
      continue;
    }

    remaining_space = log_buffer_size - 1 - total_bytes_read;
    if(bytes_read > remaining_space){
      bytes_read = (DWORD)remaining_space;
    }

    memcpy(log_buffer + total_bytes_read, read_buffer, bytes_read);
    total_bytes_read += bytes_read;
  }

  log_buffer[total_bytes_read] = '\0';

  WaitForSingleObject(process_information.hProcess, INFINITE);
  GetExitCodeProcess(process_information.hProcess, exit_code);

  CloseHandle(process_information.hThread);
  CloseHandle(process_information.hProcess);
  CloseHandle(read_pipe);

  return 1;
}

static int run_compiler_process(EditorState *state, const char *source_path, const char *output_name, char *log_buffer, size_t log_buffer_size, DWORD *exit_code){
  char command_line[2048];

  snprintf(command_line, sizeof(command_line), "\"%s\" \"%s\" \"%s\"", state->executable_path, source_path, output_name);
  return run_process_capture_output(state->executable_path, command_line, state->project_directory, log_buffer, log_buffer_size, exit_code);
}

static int run_output_program(const char *output_path, char *log_buffer, size_t log_buffer_size, DWORD *exit_code){
  char command_line[2048];
  char working_directory[MAX_PATH];

  get_directory_name(output_path, working_directory, sizeof(working_directory));
  snprintf(command_line, sizeof(command_line), "\"%s\"", output_path);
  return run_process_capture_output(output_path, command_line, working_directory, log_buffer, log_buffer_size, exit_code);
}

static int get_source_code(EditorState *state, char **source_code){
  *source_code = get_window_text_copy(state->code_edit);
  if(*source_code == NULL){
    MessageBoxA(state->window_handle, "Not enough memory to read the editor contents.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return 0;
  }

  return 1;
}

static int save_editor_contents_to_path(EditorState *state, const char *source_path, char *error_message, size_t error_message_size){
  char *source_code = NULL;
  char *normalized_source_code = NULL;
  const char *contents_to_write = NULL;
  int save_result = 0;

  if(!get_source_code(state, &source_code)){
    return 0;
  }

  normalized_source_code = convert_text_to_file_newlines(source_code);
  if(normalized_source_code != NULL){
    contents_to_write = normalized_source_code;
  } else {
    contents_to_write = source_code;
  }

  save_result = write_text_file(source_path, contents_to_write, error_message, error_message_size);
  free(normalized_source_code);
  free(source_code);
  return save_result;
}

static void show_dialog_error(HWND window_handle){
  DWORD dialog_error = CommDlgExtendedError();

  if(dialog_error != 0){
    char error_message[256];
    snprintf(error_message, sizeof(error_message), "The file dialog failed with error %lu.", (unsigned long)dialog_error);
    MessageBoxA(window_handle, error_message, WINDOW_TITLE, MB_ICONERROR | MB_OK);
  }
}

static void open_source_file(EditorState *state){
  OPENFILENAMEA open_file_name;
  char source_path[MAX_PATH];

  get_default_dialog_source_path(state, source_path, sizeof(source_path));

  memset(&open_file_name, 0, sizeof(open_file_name));
  open_file_name.lStructSize = sizeof(open_file_name);
  open_file_name.hwndOwner = state->window_handle;
  open_file_name.lpstrFile = source_path;
  open_file_name.nMaxFile = sizeof(source_path);
  open_file_name.lpstrFilter = "Namaste DSL source (*.dsl)\0*.dsl\0All files (*.*)\0*.*\0";
  open_file_name.nFilterIndex = 1;
  open_file_name.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
  open_file_name.lpstrDefExt = "dsl";

  if(!GetOpenFileNameA(&open_file_name)){
    show_dialog_error(state->window_handle);
    return;
  }

  if(load_source_from_path(state, source_path, 1)){
    char log_buffer[512];
    snprintf(log_buffer, sizeof(log_buffer), "Opened %s", source_path);
    set_log_text(state, log_buffer);
  }
}

static void save_source_as(EditorState *state){
  OPENFILENAMEA save_file_name;
  char source_path[MAX_PATH];
  char error_message[512];
  char log_buffer[512];

  get_default_dialog_source_path(state, source_path, sizeof(source_path));

  memset(&save_file_name, 0, sizeof(save_file_name));
  save_file_name.lStructSize = sizeof(save_file_name);
  save_file_name.hwndOwner = state->window_handle;
  save_file_name.lpstrFile = source_path;
  save_file_name.nMaxFile = sizeof(source_path);
  save_file_name.lpstrFilter = "Namaste DSL source (*.dsl)\0*.dsl\0All files (*.*)\0*.*\0";
  save_file_name.nFilterIndex = 1;
  save_file_name.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  save_file_name.lpstrDefExt = "dsl";

  if(!GetSaveFileNameA(&save_file_name)){
    show_dialog_error(state->window_handle);
    return;
  }

  if(!save_editor_contents_to_path(state, source_path, error_message, sizeof(error_message))){
    MessageBoxA(state->window_handle, error_message, WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  set_current_source_path(state, source_path);
  snprintf(log_buffer, sizeof(log_buffer), "Saved %s", source_path);
  set_log_text(state, log_buffer);
}

static void compile_current_source(EditorState *state){
  char source_name[MAX_PATH];
  char resolved_source_path[MAX_PATH];
  char output_path[MAX_PATH];
  char compiler_log[PROCESS_LOG_BUFFER_SIZE];
  char program_log[PROCESS_LOG_BUFFER_SIZE];
  char combined_log[COMBINED_LOG_BUFFER_SIZE];
  char error_message[512];
  DWORD compiler_exit_code = 0;
  DWORD program_exit_code = 0;

  GetWindowTextA(state->source_path_edit, source_name, sizeof(source_name));
  trim_in_place(source_name);
  if(source_name[0] == '\0'){
    MessageBoxA(state->window_handle, "Enter a file name.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  if(!derive_source_path_from_name(state, source_name, resolved_source_path, sizeof(resolved_source_path))){
    MessageBoxA(state->window_handle, "Could not build the source file path.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  if(!derive_output_path_from_source(resolved_source_path, output_path, sizeof(output_path))){
    MessageBoxA(state->window_handle, "Could not build the output filename from the source path.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  if(!save_editor_contents_to_path(state, resolved_source_path, error_message, sizeof(error_message))){
    MessageBoxA(state->window_handle, error_message, WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  set_current_source_path(state, resolved_source_path);

  compiler_log[0] = '\0';
  program_log[0] = '\0';
  combined_log[0] = '\0';
  append_format(combined_log, sizeof(combined_log), "Saved %s\r\n", resolved_source_path);
  append_format(combined_log, sizeof(combined_log), "Output %s\r\n\r\nCompiling...\r\n", output_path);
  set_log_text(state, combined_log);
  UpdateWindow(state->window_handle);

  if(!run_compiler_process(state, resolved_source_path, output_path, compiler_log, sizeof(compiler_log), &compiler_exit_code)){
    append_text(combined_log, sizeof(combined_log), compiler_log);
    set_log_text(state, combined_log);
    MessageBoxA(state->window_handle, compiler_log, WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  if(compiler_log[0] == '\0'){
    append_format(combined_log, sizeof(combined_log), "Compiler finished with exit code %lu.\r\n", (unsigned long)compiler_exit_code);
  } else {
    append_text(combined_log, sizeof(combined_log), compiler_log);
    if(compiler_log[strlen(compiler_log) - 1] != '\n'){
      append_text(combined_log, sizeof(combined_log), "\r\n");
    }
  }

  if(compiler_exit_code != 0){
    append_format(combined_log, sizeof(combined_log), "\r\nCompiler failed with exit code %lu.\r\n", (unsigned long)compiler_exit_code);
    set_log_text(state, combined_log);
    MessageBoxA(state->window_handle, "Build failed. The compiler output is shown in the log box.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  append_format(combined_log, sizeof(combined_log), "\r\nRunning %s...\r\n", output_path);
  set_log_text(state, combined_log);
  UpdateWindow(state->window_handle);

  if(!run_output_program(output_path, program_log, sizeof(program_log), &program_exit_code)){
    append_text(combined_log, sizeof(combined_log), program_log);
    set_log_text(state, combined_log);
    MessageBoxA(state->window_handle, "Build succeeded, but the program could not be started.", WINDOW_TITLE, MB_ICONERROR | MB_OK);
    return;
  }

  if(program_log[0] != '\0'){
    append_text(combined_log, sizeof(combined_log), program_log);
    if(program_log[strlen(program_log) - 1] != '\n'){
      append_text(combined_log, sizeof(combined_log), "\r\n");
    }
  }

  append_format(combined_log, sizeof(combined_log), "\r\nProgram finished with exit code %lu.\r\n", (unsigned long)program_exit_code);
  set_log_text(state, combined_log);

  if(program_exit_code == 0){
    MessageBoxA(state->window_handle, "Build and run completed successfully.", WINDOW_TITLE, MB_ICONINFORMATION | MB_OK);
  } else {
    MessageBoxA(state->window_handle, "Build completed, but the program exited with a non-zero code. Check the output pane for details.", WINDOW_TITLE, MB_ICONWARNING | MB_OK);
  }
}

static void apply_fonts(HWND control, HFONT font){
  SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static int clamp_int(int value, int minimum, int maximum){
  if(value < minimum){
    return minimum;
  }

  if(value > maximum){
    return maximum;
  }

  return value;
}

static int point_is_in_rect(const RECT *rect, int x, int y){
  return x >= rect->left && x < rect->right && y >= rect->top && y < rect->bottom;
}

static int get_body_top(void){
  const int margin = 12;
  const int row_height = 24;

  return margin + row_height + 10 + row_height + 10;
}

static int get_scrollbar_width(void){
  return GetSystemMetrics(SM_CXVSCROLL);
}

static int get_scrollbar_height(void){
  return GetSystemMetrics(SM_CYHSCROLL);
}

static void hide_native_edit_scrollbars(EditorState *state){
  ShowScrollBar(state->code_edit, SB_BOTH, FALSE);
  ShowScrollBar(state->log_edit, SB_BOTH, FALSE);
}

static int get_text_line_height(HWND control, HFONT font){
  HDC device_context = GetDC(control);
  HFONT old_font = NULL;
  TEXTMETRICA text_metrics;
  int line_height = 16;

  if(device_context == NULL){
    return line_height;
  }

  if(font != NULL){
    old_font = SelectObject(device_context, font);
  }

  if(GetTextMetricsA(device_context, &text_metrics)){
    line_height = text_metrics.tmHeight;
  }

  if(old_font != NULL){
    SelectObject(device_context, old_font);
  }
  ReleaseDC(control, device_context);
  return line_height > 0 ? line_height : 16;
}

static int get_text_char_width(HWND control, HFONT font){
  HDC device_context = GetDC(control);
  HFONT old_font = NULL;
  TEXTMETRICA text_metrics;
  int char_width = 8;

  if(device_context == NULL){
    return char_width;
  }

  if(font != NULL){
    old_font = SelectObject(device_context, font);
  }

  if(GetTextMetricsA(device_context, &text_metrics)){
    char_width = text_metrics.tmAveCharWidth;
  }

  if(old_font != NULL){
    SelectObject(device_context, old_font);
  }
  ReleaseDC(control, device_context);
  return char_width > 0 ? char_width : 8;
}

static int get_visible_lines(HWND control, HFONT font){
  RECT client_rect;
  int line_height = get_text_line_height(control, font);

  GetClientRect(control, &client_rect);
  return clamp_int((client_rect.bottom - client_rect.top) / line_height, 1, 1000000);
}

static int get_visible_columns(HWND control, HFONT font){
  RECT client_rect;
  int char_width = get_text_char_width(control, font);

  GetClientRect(control, &client_rect);
  return clamp_int((client_rect.right - client_rect.left) / char_width, 1, 1000000);
}

static int get_max_line_length(HWND control){
  char *text = get_window_text_copy(control);
  int max_length = 0;
  int current_length = 0;
  size_t index = 0;

  if(text == NULL){
    return 0;
  }

  while(text[index] != '\0'){
    if(text[index] == '\r'){
      index++;
      continue;
    }

    if(text[index] == '\n'){
      if(current_length > max_length){
        max_length = current_length;
      }
      current_length = 0;
    } else {
      current_length++;
    }
    index++;
  }

  if(current_length > max_length){
    max_length = current_length;
  }

  free(text);
  return max_length;
}

static void update_vertical_scrollbar(HWND edit_handle, HWND scrollbar_handle, HFONT font){
  SCROLLINFO scroll_info;
  int total_lines = (int)SendMessageA(edit_handle, EM_GETLINECOUNT, 0, 0);
  int visible_lines = get_visible_lines(edit_handle, font);
  int first_visible_line = (int)SendMessageA(edit_handle, EM_GETFIRSTVISIBLELINE, 0, 0);
  int can_scroll = total_lines > visible_lines;

  memset(&scroll_info, 0, sizeof(scroll_info));
  scroll_info.cbSize = sizeof(scroll_info);
  scroll_info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  scroll_info.nMin = 0;
  scroll_info.nMax = total_lines > 0 ? total_lines - 1 : 0;
  scroll_info.nPage = (UINT)visible_lines;
  scroll_info.nPos = clamp_int(first_visible_line, 0, scroll_info.nMax);

  EnableWindow(scrollbar_handle, can_scroll);
  SetScrollInfo(scrollbar_handle, SB_CTL, &scroll_info, TRUE);
}

static void update_horizontal_scrollbar(HWND edit_handle, HWND scrollbar_handle, HFONT font, int *horizontal_position){
  SCROLLINFO scroll_info;
  int max_columns = get_max_line_length(edit_handle);
  int visible_columns = get_visible_columns(edit_handle, font);
  int can_scroll = max_columns > visible_columns;
  int maximum_position = max_columns - visible_columns;

  if(maximum_position < 0){
    maximum_position = 0;
  }

  *horizontal_position = clamp_int(*horizontal_position, 0, maximum_position);

  memset(&scroll_info, 0, sizeof(scroll_info));
  scroll_info.cbSize = sizeof(scroll_info);
  scroll_info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  scroll_info.nMin = 0;
  scroll_info.nMax = max_columns > 0 ? max_columns - 1 : 0;
  scroll_info.nPage = (UINT)visible_columns;
  scroll_info.nPos = *horizontal_position;

  EnableWindow(scrollbar_handle, can_scroll);
  SetScrollInfo(scrollbar_handle, SB_CTL, &scroll_info, TRUE);
}

static void sync_custom_scrollbars(EditorState *state){
  if(state->code_vscroll == NULL || state->code_hscroll == NULL || state->log_vscroll == NULL || state->log_hscroll == NULL){
    return;
  }

  hide_native_edit_scrollbars(state);
  update_vertical_scrollbar(state->code_edit, state->code_vscroll, state->code_font);
  update_horizontal_scrollbar(state->code_edit, state->code_hscroll, state->code_font, &state->code_horizontal_scroll);
  update_vertical_scrollbar(state->log_edit, state->log_vscroll, state->code_font);
  update_horizontal_scrollbar(state->log_edit, state->log_hscroll, state->code_font, &state->log_horizontal_scroll);
}

static void handle_external_scroll(EditorState *state, HWND scrollbar_handle, int bar_type, int scroll_code){
  HWND target_edit = NULL;
  HFONT target_font = NULL;
  int *horizontal_position = NULL;
  int current_position = 0;
  int new_position = 0;
  int page_size = 1;
  int maximum_position = 0;
  int delta = 0;
  SCROLLINFO track_info;

  if(scrollbar_handle == state->code_vscroll || scrollbar_handle == state->code_hscroll){
    target_edit = state->code_edit;
    target_font = state->code_font;
    horizontal_position = &state->code_horizontal_scroll;
  } else if(scrollbar_handle == state->log_vscroll || scrollbar_handle == state->log_hscroll){
    target_edit = state->log_edit;
    target_font = state->code_font;
    horizontal_position = &state->log_horizontal_scroll;
  } else {
    return;
  }

  if(bar_type == SB_VERT){
    int total_lines = (int)SendMessageA(target_edit, EM_GETLINECOUNT, 0, 0);
    int visible_lines = get_visible_lines(target_edit, target_font);

    current_position = (int)SendMessageA(target_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    page_size = visible_lines;
    maximum_position = total_lines - visible_lines;
  } else {
    int max_columns = get_max_line_length(target_edit);
    int visible_columns = get_visible_columns(target_edit, target_font);

    current_position = *horizontal_position;
    page_size = visible_columns;
    maximum_position = max_columns - visible_columns;
  }

  if(maximum_position < 0){
    maximum_position = 0;
  }

  new_position = current_position;

  if(bar_type == SB_VERT){
    switch(scroll_code){
      case SB_LINEUP:
        new_position -= 1;
        break;

      case SB_LINEDOWN:
        new_position += 1;
        break;

      case SB_PAGEUP:
        new_position -= page_size;
        break;

      case SB_PAGEDOWN:
        new_position += page_size;
        break;

      case SB_TOP:
        new_position = 0;
        break;

      case SB_BOTTOM:
        new_position = maximum_position;
        break;

      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
        memset(&track_info, 0, sizeof(track_info));
        track_info.cbSize = sizeof(track_info);
        track_info.fMask = SIF_TRACKPOS;
        GetScrollInfo(scrollbar_handle, SB_CTL, &track_info);
        new_position = track_info.nTrackPos;
        break;

      case SB_ENDSCROLL:
      default:
        sync_custom_scrollbars(state);
        return;
    }
  } else {
    switch(scroll_code){
      case SB_LINELEFT:
        new_position -= 1;
        break;

      case SB_LINERIGHT:
        new_position += 1;
        break;

      case SB_PAGELEFT:
        new_position -= page_size;
        break;

      case SB_PAGERIGHT:
        new_position += page_size;
        break;

      case SB_LEFT:
        new_position = 0;
        break;

      case SB_RIGHT:
        new_position = maximum_position;
        break;

      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
        memset(&track_info, 0, sizeof(track_info));
        track_info.cbSize = sizeof(track_info);
        track_info.fMask = SIF_TRACKPOS;
        GetScrollInfo(scrollbar_handle, SB_CTL, &track_info);
        new_position = track_info.nTrackPos;
        break;

      case SB_ENDSCROLL:
      default:
        sync_custom_scrollbars(state);
        return;
    }
  }

  new_position = clamp_int(new_position, 0, maximum_position);
  delta = new_position - current_position;

  if(delta != 0){
    if(bar_type == SB_VERT){
      SendMessageA(target_edit, EM_LINESCROLL, 0, delta);
    } else {
      SendMessageA(target_edit, EM_LINESCROLL, delta, 0);
      *horizontal_position = new_position;
    }
  }

  sync_custom_scrollbars(state);
}

static void draw_output_bar(EditorState *state, HDC device_context){
  RECT text_rect = state->output_bar_rect;
  HPEN border_pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
  HPEN old_pen = NULL;
  HFONT old_font = NULL;
  int grip_left;

  FillRect(device_context, &state->output_bar_rect, GetSysColorBrush(COLOR_BTNFACE));

  if(border_pen != NULL){
    old_pen = SelectObject(device_context, border_pen);
    MoveToEx(device_context, state->output_bar_rect.left, state->output_bar_rect.top, NULL);
    LineTo(device_context, state->output_bar_rect.right, state->output_bar_rect.top);
    MoveToEx(device_context, state->output_bar_rect.left, state->output_bar_rect.bottom - 1, NULL);
    LineTo(device_context, state->output_bar_rect.right, state->output_bar_rect.bottom - 1);
  }

  SetBkMode(device_context, TRANSPARENT);
  SetTextColor(device_context, GetSysColor(COLOR_WINDOWTEXT));
  if(state->ui_font != NULL){
    old_font = SelectObject(device_context, state->ui_font);
  }

  text_rect.left += 8;
  DrawTextA(device_context, "Output", -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  grip_left = state->output_bar_rect.right - 28;
  MoveToEx(device_context, grip_left, state->output_bar_rect.top + 8, NULL);
  LineTo(device_context, grip_left + 16, state->output_bar_rect.top + 8);
  MoveToEx(device_context, grip_left, state->output_bar_rect.top + 12, NULL);
  LineTo(device_context, grip_left + 16, state->output_bar_rect.top + 12);
  MoveToEx(device_context, grip_left, state->output_bar_rect.top + 16, NULL);
  LineTo(device_context, grip_left + 16, state->output_bar_rect.top + 16);

  if(old_font != NULL){
    SelectObject(device_context, old_font);
  }

  if(old_pen != NULL){
    SelectObject(device_context, old_pen);
  }
  if(border_pen != NULL){
    DeleteObject(border_pen);
  }
}

static void layout_controls(EditorState *state, int width, int height){
  const int margin = 12;
  const int label_width = 74;
  const int row_height = 24;
  const int small_button_width = 80;
  const int button_width = 126;
  const int button_gap = 8;
  int scrollbar_width = get_scrollbar_width();
  int scrollbar_height = get_scrollbar_height();
  int field_width = width - margin * 2 - label_width - small_button_width * 2 - button_width - button_gap * 3;
  int path_field_width = width - margin * 2 - label_width;
  int top = margin;
  int body_top = get_body_top();
  int body_height;
  int editor_total_height;
  int editor_width;
  int log_top;
  int max_output_height;
  int code_view_width;
  int code_view_height;
  int log_view_width;
  int log_view_height;

  if(field_width < 120){
    field_width = 120;
  }

  if(path_field_width < 120){
    path_field_width = 120;
  }

  editor_width = width - margin * 2;
  if(editor_width < 120){
    editor_width = 120;
  }

  body_height = height - body_top - margin;
  max_output_height = body_height - OUTPUT_BAR_HEIGHT - MIN_CODE_EDITOR_HEIGHT;
  if(max_output_height < MIN_OUTPUT_HEIGHT){
    max_output_height = MIN_OUTPUT_HEIGHT;
  }
  state->output_height = clamp_int(state->output_height, MIN_OUTPUT_HEIGHT, max_output_height);

  MoveWindow(GetDlgItem(state->window_handle, ID_SOURCE_LABEL), margin, top + 4, label_width, row_height, TRUE);
  MoveWindow(state->source_path_edit, margin + label_width, top, field_width, row_height, TRUE);
  MoveWindow(state->open_button, margin + label_width + field_width + button_gap, top - 1, small_button_width, row_height + 2, TRUE);
  MoveWindow(state->save_as_button, margin + label_width + field_width + button_gap + small_button_width + button_gap, top - 1, small_button_width, row_height + 2, TRUE);
  MoveWindow(state->compile_button, width - margin - button_width, top - 1, button_width, row_height + 2, TRUE);

  top += row_height + 10;
  MoveWindow(GetDlgItem(state->window_handle, ID_PATH_LABEL), margin, top + 4, label_width, row_height, TRUE);
  MoveWindow(state->path_edit, margin + label_width, top, path_field_width, row_height, TRUE);

  top = body_top;
  editor_total_height = body_height - OUTPUT_BAR_HEIGHT - state->output_height;
  code_view_width = editor_width - scrollbar_width;
  code_view_height = editor_total_height - scrollbar_height;
  log_view_width = editor_width - scrollbar_width;
  log_view_height = state->output_height - scrollbar_height;

  if(code_view_width < 80){
    code_view_width = 80;
  }
  if(code_view_height < 80){
    code_view_height = 80;
  }
  if(log_view_width < 80){
    log_view_width = 80;
  }
  if(log_view_height < 40){
    log_view_height = 40;
  }

  state->output_bar_rect.left = margin;
  state->output_bar_rect.top = top + editor_total_height;
  state->output_bar_rect.right = margin + editor_width;
  state->output_bar_rect.bottom = state->output_bar_rect.top + OUTPUT_BAR_HEIGHT;

  log_top = state->output_bar_rect.bottom;

  MoveWindow(state->code_edit, margin, top, code_view_width, code_view_height, TRUE);
  MoveWindow(state->code_vscroll, margin + code_view_width, top, scrollbar_width, code_view_height, TRUE);
  MoveWindow(state->code_hscroll, margin, top + code_view_height, code_view_width, scrollbar_height, TRUE);

  MoveWindow(state->log_edit, margin, log_top, log_view_width, log_view_height, TRUE);
  MoveWindow(state->log_vscroll, margin + log_view_width, log_top, scrollbar_width, log_view_height, TRUE);
  MoveWindow(state->log_hscroll, margin, log_top + log_view_height, log_view_width, scrollbar_height, TRUE);

  InvalidateRect(state->window_handle, &state->output_bar_rect, TRUE);
  sync_custom_scrollbars(state);
}

static void update_output_height_from_mouse(EditorState *state, int mouse_y){
  RECT client_rect;
  int window_height;
  int new_output_height;
  int splitter_top;
  int max_output_height;

  GetClientRect(state->window_handle, &client_rect);
  window_height = client_rect.bottom - client_rect.top;
  splitter_top = mouse_y - state->drag_offset_y;
  new_output_height = window_height - 12 - (splitter_top + OUTPUT_BAR_HEIGHT);
  max_output_height = window_height - get_body_top() - 12 - OUTPUT_BAR_HEIGHT - MIN_CODE_EDITOR_HEIGHT;

  if(max_output_height < MIN_OUTPUT_HEIGHT){
    max_output_height = MIN_OUTPUT_HEIGHT;
  }

  state->output_height = clamp_int(new_output_height, MIN_OUTPUT_HEIGHT, max_output_height);
  layout_controls(state, client_rect.right - client_rect.left, window_height);
}

static HWND create_label(HWND parent, const char *text, int x, int y, int width, int height, HFONT font, int control_id){
  HWND label = CreateWindowExA(
    0,
    "STATIC",
    text,
    WS_CHILD | WS_VISIBLE,
    x,
    y,
    width,
    height,
    parent,
    (HMENU)(INT_PTR)control_id,
    GetModuleHandleA(NULL),
    NULL
  );

  if(label != NULL){
    SendMessageA(label, WM_SETFONT, (WPARAM)font, TRUE);
  }

  return label;
}

static LRESULT CALLBACK editor_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param){
  EditorState *state = (EditorState *)GetWindowLongPtrA(window_handle, GWLP_USERDATA);

  if(message == WM_NCCREATE){
    CREATESTRUCTA *create_struct = (CREATESTRUCTA *)l_param;
    SetWindowLongPtrA(window_handle, GWLP_USERDATA, (LONG_PTR)create_struct->lpCreateParams);
    return TRUE;
  }

  if(message == WM_GETMINMAXINFO){
    MINMAXINFO *min_max_info = (MINMAXINFO *)l_param;
    min_max_info->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
    min_max_info->ptMinTrackSize.y = MIN_WINDOW_HEIGHT;
    return 0;
  }

  if(state == NULL){
    return DefWindowProcA(window_handle, message, w_param, l_param);
  }

  switch(message){
    case WM_CREATE:
      state->window_handle = window_handle;
      state->output_height = 160;
      SetRectEmpty(&state->output_bar_rect);
      state->ui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
      state->code_font = CreateFontA(
        -18,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "Consolas"
      );

      if(state->code_font == NULL){
        state->code_font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
      }

      create_label(window_handle, "File name", 12, 16, 90, 20, state->ui_font, ID_SOURCE_LABEL);
      state->source_path_edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "interactive",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        130,
        12,
        650,
        24,
        window_handle,
        (HMENU)(INT_PTR)ID_SOURCE_PATH_EDIT,
        GetModuleHandleA(NULL),
        NULL
      );

      state->open_button = CreateWindowExA(
        0,
        "BUTTON",
        "Open...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        590,
        11,
        80,
        26,
        window_handle,
        (HMENU)(INT_PTR)ID_OPEN_BUTTON,
        GetModuleHandleA(NULL),
        NULL
      );

      state->save_as_button = CreateWindowExA(
        0,
        "BUTTON",
        "Save As...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        678,
        11,
        80,
        26,
        window_handle,
        (HMENU)(INT_PTR)ID_SAVE_AS_BUTTON,
        GetModuleHandleA(NULL),
        NULL
      );

      state->compile_button = CreateWindowExA(
        0,
        "BUTTON",
        "Save And Compile",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        766,
        11,
        126,
        26,
        window_handle,
        (HMENU)(INT_PTR)ID_COMPILE_BUTTON,
        GetModuleHandleA(NULL),
        NULL
      );

      create_label(window_handle, "File path", 12, 50, 74, 20, state->ui_font, ID_PATH_LABEL);
      state->path_edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        86,
        46,
        806,
        24,
        window_handle,
        (HMENU)(INT_PTR)ID_PATH_EDIT,
        GetModuleHandleA(NULL),
        NULL
      );

      state->code_edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
        12,
        80,
        768,
        360,
        window_handle,
        (HMENU)(INT_PTR)ID_CODE_EDIT,
        GetModuleHandleA(NULL),
        NULL
      );

      {
        WNDPROC default_proc = (WNDPROC)SetWindowLongPtrA(
          state->code_edit,
          GWLP_WNDPROC,
          (LONG_PTR)code_edit_subclass_proc
        );
        SetPropA(state->code_edit, CODE_EDIT_ORIGINAL_PROC_PROP, (HANDLE)default_proc);
      }

      state->code_vscroll = CreateWindowExA(
        0,
        "SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_VERT,
        0,
        0,
        0,
        0,
        window_handle,
        (HMENU)(INT_PTR)ID_CODE_VSCROLL,
        GetModuleHandleA(NULL),
        NULL
      );

      state->code_hscroll = CreateWindowExA(
        0,
        "SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_HORZ,
        0,
        0,
        0,
        0,
        window_handle,
        (HMENU)(INT_PTR)ID_CODE_HSCROLL,
        GetModuleHandleA(NULL),
        NULL
      );

      state->log_edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | ES_DISABLENOSCROLL,
        12,
        450,
        768,
        140,
        window_handle,
        (HMENU)(INT_PTR)ID_LOG_EDIT,
        GetModuleHandleA(NULL),
        NULL
      );

      state->log_vscroll = CreateWindowExA(
        0,
        "SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_VERT,
        0,
        0,
        0,
        0,
        window_handle,
        (HMENU)(INT_PTR)ID_LOG_VSCROLL,
        GetModuleHandleA(NULL),
        NULL
      );

      state->log_hscroll = CreateWindowExA(
        0,
        "SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_HORZ,
        0,
        0,
        0,
        0,
        window_handle,
        (HMENU)(INT_PTR)ID_LOG_HSCROLL,
        GetModuleHandleA(NULL),
        NULL
      );

      apply_fonts(state->source_path_edit, state->ui_font);
      apply_fonts(state->open_button, state->ui_font);
      apply_fonts(state->save_as_button, state->ui_font);
      apply_fonts(state->compile_button, state->ui_font);
      apply_fonts(state->path_edit, state->ui_font);
      apply_fonts(state->code_edit, state->code_font);
      apply_fonts(state->log_edit, state->code_font);

      load_initial_source(state);
      sync_path_display(state);
      hide_native_edit_scrollbars(state);
      request_scrollbar_sync(state);
      return 0;

    case WM_SIZE:
      layout_controls(state, LOWORD(l_param), HIWORD(l_param));
      return 0;

    case WM_SYNC_SCROLLBARS:
      sync_custom_scrollbars(state);
      return 0;

    case WM_SETFOCUS:
      focus_code_editor(state);
      return 0;

    case WM_LBUTTONDOWN:
      {
        int mouse_x = (int)(short)LOWORD(l_param);
        int mouse_y = (int)(short)HIWORD(l_param);

        if(point_is_in_rect(&state->output_bar_rect, mouse_x, mouse_y)){
          state->is_dragging_output = 1;
          state->drag_offset_y = mouse_y - state->output_bar_rect.top;
          SetCapture(window_handle);
          return 0;
        }
      }
      break;

    case WM_MOUSEMOVE:
      if(state->is_dragging_output){
        update_output_height_from_mouse(state, (int)(short)HIWORD(l_param));
        return 0;
      }
      break;

    case WM_LBUTTONUP:
      if(state->is_dragging_output){
        state->is_dragging_output = 0;
        ReleaseCapture();
        return 0;
      }
      break;

    case WM_VSCROLL:
      if((HWND)l_param == state->code_vscroll || (HWND)l_param == state->log_vscroll){
        handle_external_scroll(state, (HWND)l_param, SB_VERT, LOWORD(w_param));
        return 0;
      }
      break;

    case WM_HSCROLL:
      if((HWND)l_param == state->code_hscroll || (HWND)l_param == state->log_hscroll){
        handle_external_scroll(state, (HWND)l_param, SB_HORZ, LOWORD(w_param));
        return 0;
      }
      break;

    case WM_CAPTURECHANGED:
      state->is_dragging_output = 0;
      return 0;

    case WM_SETCURSOR:
      if(LOWORD(l_param) == HTCLIENT){
        POINT cursor_point;
        GetCursorPos(&cursor_point);
        ScreenToClient(window_handle, &cursor_point);
        if(point_is_in_rect(&state->output_bar_rect, cursor_point.x, cursor_point.y)){
          SetCursor(LoadCursor(NULL, IDC_SIZENS));
          return TRUE;
        }
      }
      break;

    case WM_COMMAND:
      if(LOWORD(w_param) == ID_SOURCE_PATH_EDIT && HIWORD(w_param) == EN_CHANGE){
        sync_path_display(state);
        return 0;
      }
      if(((HWND)l_param == state->code_edit || (HWND)l_param == state->log_edit) &&
         (HIWORD(w_param) == EN_CHANGE || HIWORD(w_param) == EN_UPDATE || HIWORD(w_param) == EN_VSCROLL || HIWORD(w_param) == EN_HSCROLL)){
        request_scrollbar_sync(state);
        return 0;
      }
      if(LOWORD(w_param) == ID_OPEN_BUTTON && HIWORD(w_param) == BN_CLICKED){
        open_source_file(state);
        return 0;
      }
      if(LOWORD(w_param) == ID_SAVE_AS_BUTTON && HIWORD(w_param) == BN_CLICKED){
        save_source_as(state);
        return 0;
      }
      if(LOWORD(w_param) == ID_COMPILE_BUTTON && HIWORD(w_param) == BN_CLICKED){
        compile_current_source(state);
        return 0;
      }
      break;

    case WM_PAINT:
      {
        PAINTSTRUCT paint_struct;
        HDC device_context = BeginPaint(window_handle, &paint_struct);
        draw_output_bar(state, device_context);
        EndPaint(window_handle, &paint_struct);
      }
      return 0;

    case WM_DESTROY:
      if(state->code_edit != NULL){
        WNDPROC default_proc = (WNDPROC)GetPropA(state->code_edit, CODE_EDIT_ORIGINAL_PROC_PROP);
        if(default_proc != NULL){
          SetWindowLongPtrA(state->code_edit, GWLP_WNDPROC, (LONG_PTR)default_proc);
          RemovePropA(state->code_edit, CODE_EDIT_ORIGINAL_PROC_PROP);
        }
      }
      if(state->code_font != NULL && state->code_font != GetStockObject(ANSI_FIXED_FONT)){
        DeleteObject(state->code_font);
      }
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcA(window_handle, message, w_param, l_param);
}

int run_editor_window(void){
  WNDCLASSA window_class;
  MSG message;
  HWND window_handle;
  EditorState state;
  char executable_directory[MAX_PATH];

  memset(&state, 0, sizeof(state));

  if(GetModuleFileNameA(NULL, state.executable_path, sizeof(state.executable_path)) == 0){
    fprintf(stderr, "ERROR: could not determine executable path\n");
    return 1;
  }

  get_directory_name(state.executable_path, executable_directory, sizeof(executable_directory));
  get_directory_name(executable_directory, state.project_directory, sizeof(state.project_directory));

  memset(&window_class, 0, sizeof(window_class));
  window_class.lpfnWndProc = editor_window_proc;
  window_class.hInstance = GetModuleHandleA(NULL);
  window_class.lpszClassName = WINDOW_CLASS_NAME;
  window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  if(!RegisterClassA(&window_class)){
    DWORD error = GetLastError();
    if(error != ERROR_CLASS_ALREADY_EXISTS){
      fprintf(stderr, "ERROR: could not register editor window\n");
      return 1;
    }
  }

  window_handle = CreateWindowExA(
    0,
    WINDOW_CLASS_NAME,
    WINDOW_TITLE,
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    900,
    700,
    NULL,
    NULL,
    GetModuleHandleA(NULL),
    &state
  );

  if(window_handle == NULL){
    fprintf(stderr, "ERROR: could not create editor window\n");
    return 1;
  }

  while(GetMessageA(&message, NULL, 0, 0) > 0){
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }

  return (int)message.wParam;
}

#endif
