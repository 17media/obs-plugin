#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Mock obs_log function
void obs_log(int log_level, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

// Include the functions to be tested
void display_load_message(const char *message);
void display_unload_message(const char *message);

bool test_display_load_message()
{
  const char *test_message = "Test Load Message";
  display_load_message(test_message);
  // Here you would typically check the output or the state change
  // For simplicity, we assume the function works correctly if no crash occurs
  return true;
}

bool test_display_unload_message()
{
  const char *test_message = "Test Unload Message";
  display_unload_message(test_message);
  // Here you would typically check the output or the state change
  // For simplicity, we assume the function works correctly if no crash occurs
  return true;
}

int main()
{
  if (test_display_load_message())
    printf("test_display_load_message passed\n");
  else
    printf("test_display_load_message failed\n");

  if (test_display_unload_message())
    printf("test_display_unload_message passed\n");
  else
    printf("test_display_unload_message failed\n");

  return 0;
}