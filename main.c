#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <Windows.h>
#include <tchar.h>
#include <ctype.h>
#include <dirent.h>

#define BUFSIZE 1024
#define MAX_LINE_LEN 1024
#define MAX_LEN 1024
#define MAX_CHANGES 100
#define TAB_SIZE 4
#define MAX_LINE_LENGTH 1000



int createBackup(const char *file_name) {
  char file_backup[MAX_LEN];
  sprintf(file_backup, "%s.bak", file_name);

  FILE *fp_original = fopen(file_name, "r");
  if (!fp_original) {
    printf("Error: unable to open file %s\n", file_name);
    return 1;
  }

  FILE *fp_backup = fopen(file_backup, "w");
  if (!fp_backup) {
    printf("Error: unable to open file %s\n", file_backup);
    fclose(fp_original);
    return 2;
  }

  char line[MAX_LEN];
  while (fgets(line, MAX_LEN, fp_original)) {
    fputs(line, fp_backup);
  }

  fclose(fp_original);
  fclose(fp_backup);

  return 0;
}

char *read_file(FILE *fp) {
    char *buffer = NULL;
    long length;

    // Determine the size of the file
    if (fseek(fp, 0, SEEK_END) == 0) {
        length = ftell(fp);
        if (length == -1) {
            return NULL;
        }

        // Allocate a buffer to hold the file contents
        buffer = (char *) malloc(sizeof(char) * (length + 1));

        // Rewind the file position indicator to the start of the file
        if (fseek(fp, 0, SEEK_SET) != 0) {
            free(buffer);
            return NULL;
        }

        // Read the file contents into the buffer
        if (fread(buffer, sizeof(char), length, fp) != length) {
            free(buffer);
            return NULL;
        }

        // Add a null terminator to the end of the buffer
        buffer[length] = '\0';
    }

    return buffer;
}

void copyToClipboard(const char *text)
{
    // open the clipboard
    if (OpenClipboard(NULL))
    {
        // clear the clipboard
        EmptyClipboard();

        // allocate memory for the text
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
        if (hMem)
        {
            // lock the memory and copy the text
            char *pMem = (char *)GlobalLock(hMem);
            strcpy(pMem, text);
            GlobalUnlock(hMem);

            // set the clipboard data
            SetClipboardData(CF_TEXT, hMem);
        }

        // close the clipboard
        CloseClipboard();
    }
}

void retrieveClipboardText(char *destination, int destSize) {
  if (!OpenClipboard(NULL)) {
    _tprintf(_T("OpenClipboard failed. Error code: %d"), GetLastError());
    return;
  }

  HANDLE clipData = GetClipboardData(CF_TEXT);
  if (!clipData) {
    _tprintf(_T("GetClipboardData failed. Error code: %d"), GetLastError());
    CloseClipboard();
    return;
  }

  char *clipText = (char *)GlobalLock(clipData);
  if (!clipText) {
    _tprintf(_T("GlobalLock failed. Error code: %d"), GetLastError());
    CloseClipboard();
    return;
  }

  strncpy(destination, clipText, destSize - 1);
  destination[destSize - 1] = '\0';

  GlobalUnlock(clipData);
  CloseClipboard();
}

int find_str(const char *str, const char *file_contents, int **positions, int byword) {
    int str_len = strlen(str);
    int file_len = strlen(file_contents);
    int count = 0;
    int i, j;

    // Count the number of occurrences of the string in the file
    for (i = 0; i < file_len - str_len + 1; i++) {
        if (strncmp(file_contents + i, str, str_len) == 0) {
            count++;
        }
    }

    // Allocate memory for the positions array
    *positions = (int *) malloc(sizeof(int) * count);

    // Store the positions of the occurrences of the string in the file
    j = 0;
    for (i = 0; i < file_len - str_len + 1; i++) {
        if (strncmp(file_contents + i, str, str_len) == 0) {
            (*positions)[j++] = i;
        }
    }

    // Convert character positions to word positions if specified
    if (byword) {
        for (i = 0; i < count; i++) {
            int word_count = 0;
            int k;
            for (k = 0; k < (*positions)[i]; k++) {
                if (file_contents[k] == ' ') {
                    word_count++;
                }
            }
            (*positions)[i] = word_count + 1;
        }
    }
    return count;
}

/**
* create file
*/
int createFile(const char *file_path)
{
    char *dir_path = strdup(file_path);
    char *base_name = strrchr(dir_path, '/');

    if (base_name == NULL) {
        printf("Error: Invalid file path.\n");
        free(dir_path);
        return 0;
    }

    *base_name = '\0';
    base_name++;

    struct stat st;

    // if (stat(dir_path, &st) != 0) {
    //     if (mkdir(dir_path,'755') != 0) {
    //         printf("Error: Failed to create directory.\n");
    //         free(dir_path);
    //         return 0;
    //     }
    // } else if (!S_ISDIR(st.st_mode)) {
    //     printf("Error: Path is not a directory.\n");
    //     free(dir_path);
    //     return 0;
    // }

    FILE *fp = fopen(file_path, "r");
    if (fp != NULL) {
        printf("Error: File already exists.\n");
        fclose(fp);
        free(dir_path);
        return 0;
    }

    fp = fopen(file_path, "w");
    if (fp == NULL) {
        printf("Error: Failed to create file.\n");
        free(dir_path);
        return 0;
    }

    printf("File created successfully: %s\n", file_path);
    fclose(fp);
    free(dir_path);
    return 1;

}

/**
* insert string to file
*/
int insertStr(char *file_name, char *str, int line, int start) {
    FILE *fp;
    char buffer[BUFSIZE];
    int line_no = 1;
    int i, j;
    int len = strlen(str);
    char *new_str;
    int new_len = 0;

    fp = fopen(file_name, "r+");
    if (fp == NULL) {
        printf("Error opening file %s\n", file_name);
        return ERROR;
    }

    // Calculate the length of the new string
    for (i = 0; i < len; i++) {
        if (str[i] == '\\' && str[i + 1] == 'n') {
            new_len += 1;
            i++;
        } else if (str[i] == '\\' && str[i + 1] == '\\') {
            new_len += 1;
            i++;
        } else {
            new_len += 1;
        }
    }

    // Allocate memory for the new string
    new_str = (char *)malloc(new_len + 1);
    if (new_str == NULL) {
        printf("Error allocating memory for new string\n");
        fclose(fp);
        return ERROR;
    }

    // Create the new string
    for (i = 0, j = 0; i < len; i++, j++) {
        if (str[i] == '\\' && str[i + 1] == 'n') {
            new_str[j] = '\n';
            i++;
        } else if (str[i] == '\\' && str[i + 1] == '\\') {
            new_str[j] = '\\';
            i++;
        } else {
            new_str[j] = str[i];
        }
    }
    new_str[new_len] = '\0';

    // Move the file pointer to the desired line
    while (line_no < line) {
        if (fgets(buffer, BUFSIZE, fp) == NULL) {
            printf("Line number %d does not exist\n", line);
            fclose(fp);
            free(new_str);
            return ERROR;
        }
        line_no++;
    }

    // Move the file pointer to the desired start position
    fseek(fp, start, SEEK_CUR);

    // Insert the new string into the file
    fwrite(new_str, 1, new_len, fp);

    fclose(fp);
    free(new_str);
    return 0;
}

int cat(char *file_name){

  FILE *file = fopen(file_name, "r");
  if (!file) {
    printf("Error: unable to open file %s\n", file_name);
    return 1;
  }

  int c;
  while ((c = fgetc(file)) != EOF) {
    putchar(c);
  }

  fclose(file);
  return 0;
}

int removeStr(const char *fileName, int line, int startPos, int size, char direction) {
    // Open the file for reading and writing
    FILE *fp = fopen(fileName, "r+");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Determine the size of the file
    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);
    rewind(fp);

    // Read the file line by line into a buffer
    char buffer[fileSize];
    int bufferIndex = 0;
    char lineBuffer[BUFSIZE];
    int lineNum = 0;
    while (fgets(lineBuffer, BUFSIZE, fp)) {
        lineNum++;

        // If this is the target line, remove the specified characters
        if (lineNum == line) {
            int lineLength = strlen(lineBuffer);
            if (direction == 'b') {
                // Remove the characters from the end of the line
                lineBuffer[lineLength - size] = '\0';
            } else {
                // Remove the characters from the start of the line
                memmove(lineBuffer, lineBuffer + size, lineLength - size + 1);
            }
        }

        // Copy the line into the buffer
        int lineLength = strlen(lineBuffer);
        memcpy(buffer + bufferIndex, lineBuffer, lineLength);
        bufferIndex += lineLength;
    }

    // Write the modified buffer back to the file
    rewind(fp);
    fwrite(buffer, 1, bufferIndex, fp);

    // Truncate the file to remove any extra characters that were not written
    ftruncate(fileno(fp), bufferIndex);

    // Close the file
    fclose(fp);

    return 0;
}

char *readLine(FILE *fp, int lineNo) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int i = 1;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (i == lineNo) {
            return line;
        }
        i++;
    }
    return NULL;
}

int copyStr(const char *fileName, int line, int startPos, int size, char direction) {
    FILE *fp = fopen(fileName, "r+");
    if (fp == NULL) {
        return 1;
    }

    char *srcLine = readLine(fp, line);
    if (srcLine == NULL) {
        fclose(fp);
        return 2;
    }

    int srcLen = strlen(srcLine);
    if (startPos < 0 || startPos >= srcLen) {
        free(srcLine);
        fclose(fp);
        return 3;
    }

    int endPos = startPos + size;
    if (endPos < 0 || endPos > srcLen) {
        endPos = srcLen;
    }
    int copyLen = endPos - startPos;

    char *copyBuf = (char *)malloc(copyLen + 1);
    memcpy(copyBuf, srcLine + startPos, copyLen);
    copyBuf[copyLen] = '\0';
    free(srcLine);

    if (direction == 'b') {
        memmove(copyBuf + copyLen + 1, copyBuf, copyLen);
        copyBuf[0] = '\n';
        copyLen++;
    }

    char *dstLine = readLine(fp, line);
    if (dstLine == NULL) {
        free(copyBuf);
        fclose(fp);
        return 4;
    }

    int dstLen = strlen(dstLine);
    if (startPos < 0 || startPos > dstLen) {
        startPos = dstLen;
    }

    int newDstLen = dstLen + copyLen;
    dstLine = (char *)realloc(dstLine, newDstLen + 1);
    memmove(dstLine + startPos + copyLen, dstLine + startPos, dstLen - startPos + 1);
    memcpy(dstLine + startPos, copyBuf, copyLen);
    dstLine[newDstLen] = '\0';

    free(copyBuf);

    int pos = 0;
    fseek(fp, 0, SEEK_SET);
    for (int i = 1; i < line; i++) {
        char buf[BUFSIZE];
        if (fgets(buf, BUFSIZE, fp) == NULL) {
            break;
        }
        pos += strlen(buf);
    }
    fseek(fp, pos, SEEK_SET);
    fwrite(dstLine, sizeof(char), newDstLen, fp);
    fclose(fp);
    return 0;
}

int cutStr(const char *fileName, int line, int startPos, int size, char direction) {
    char buf[BUFSIZE];
    int curr_line = 1;
    int curr_pos = 0;
    int found = 0;

    // open the file
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        printf("Error opening file %s\n", fileName);
        return 1;
    }

    // find the desired text
    while (fgets(buf, BUFSIZE, fp) != NULL)
    {
        if (curr_line == line)
        {
            found = 1;
            break;
        }
        curr_line++;
    }
    fclose(fp);

    // check if line number is valid
    if (!found)
    {
        printf("Line number %d not found in file %s\n", line, fileName);
        return 1;
    }

    // check if start position is valid
    int len = strlen(buf);
    if (startPos < 0 || startPos >= len)
    {
        printf("Invalid start position %d for line %d in file %s\n", startPos, line, fileName);
        return 1;
    }

    // check if size is valid
    if (size < 0 || size > len - startPos)
    {
        printf("Invalid size %d for start position %d in line %d of file %s\n", size, startPos, line, fileName);
        return 1;
    }

    // copy the desired text to the clipboard
    char *text = malloc(size + 1);
    if (direction == 'f')
    {
        strncpy(text, buf + startPos, size);
    }
    else
    {
        strncpy(text, buf + startPos - size + 1, size);
    }
    text[size] = '\0';
    copyToClipboard(text);
    //printf("Copied to clipboard: %s\n", text);

    // delete the desired text from the file
    int start, end;
    if (direction == 'f')
    {
        start = startPos;
        end = startPos + size;
    }
    else
    {
        start = startPos - size + 1;
        end = startPos + 1;
    }
fp = fopen(fileName, "r");
    FILE *tmp = tmpfile();
    while (fgets(buf, BUFSIZE, fp) != NULL)
    {
        if (curr_line == line)
        {
            fwrite(buf, 1, start, tmp);
            fseek(fp, end - start, SEEK_CUR);
        }
        else
        {
            fputs(buf, tmp);
        }
        curr_line++;
    }
    fclose(fp);
    fclose(tmp);

    fp = fopen(fileName, "w");
    tmp = fopen(fileName, "r");
    while (fgets(buf, BUFSIZE, tmp) != NULL)
    {
          fputs(buf, fp);
    }
    fclose(fp);
    fclose(tmp);

    return 0;
}

void pasteStr(const char *filename, int line, int pos) {

  FILE *file = fopen(filename, "r+");
  if (!file) {
    perror("Error opening file");
    return;
  }


  char buffer[1024];
  int currentLine = 1;

  while (fgets(buffer, sizeof buffer, file)) {
    if (currentLine == line) {
      int len = strlen(buffer);
      if (pos > len) {
        fprintf(stderr, "Error: position %d is greater than line length %d\n", pos, len);
        fclose(file);
        return;
      }

      char clipBoard[1024];
      //retrieveClipboardText(clipBoard, sizeof(clipBoard));
      //printf("Clipboard text: %s");

      strncpy(clipBoard, buffer, pos);
      clipBoard[pos] = '\0';
      strcat(clipBoard, "clipBoard");
      strcat(clipBoard, buffer + pos);
      fseek(file, -len, SEEK_CUR);
      fprintf(file, "%s", clipBoard);
      fclose(file);
      return;
    }
    currentLine++;
  }
  fprintf(stderr, "Error: line %d not found in file\n", line);
  fclose(file);
}


int count_occurrences(char *file_content, char *search_string) {
  int count = 0;
  char *occurrence = file_content;

  while ((occurrence = strstr(occurrence, search_string)) != NULL) {
    count++;
    occurrence += strlen(search_string);
  }

  return count;
}


int find_occurrence(const char *str, const char *file_name, int count, int at, int byword, int all) {
  FILE *fp = fopen(file_name, "r");
  if (!fp) {
    printf("Error opening file\n");
    return -1;
  }

  int found_count = 0;
  int result = -1;
  int word_count = 1;
  char line[MAX_LINE_LENGTH];

  while (fgets(line, MAX_LINE_LENGTH, fp)) {
    char *occurrence = strstr(line, str);
    while (occurrence) {
      int position = occurrence - line;
      found_count++;

      if (count) {
        if (found_count == at) {
          result = position;
          break;
        }
      } else if (all) {
        printf("%d, ", word_count);
      } else {
        result = position;
        break;
      }

      occurrence = strstr(occurrence + 1, str);
    }

    if (byword) {
      char *token = strtok(line, " ");
      while (token) {
        if (strcmp(token, str) == 0) {
          found_count++;
          if (count) {
            if (found_count == at) {
              result = word_count;
              break;
            }
          } else if (all) {
            printf("%d, ", word_count);
          } else {
            result = word_count;
            break;
          }
        }
        token = strtok(NULL, " ");
        word_count++;
      }
    }

    if (result != -1) {
      break;
    }
  }

  fclose(fp);
  if (all) {
    printf("\n");
  }

  if (found_count == 0) {
    printf("Expression not found in file\n");
    return -1;
  }

  if (count && found_count < at) {
    printf("Expression found %d times in file, not enough to satisfy '-at %d' option\n", found_count, at);
    return -1;
  }

  return result;
}

void replace(char *filename, char *str1, char *str2, int flag, int at) {
    FILE *fp;
    char buffer[1024];
    int count = 0;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: Unable to open file %s\n", filename);
        return;
    }

    while (fgets(buffer, 1024, fp) != NULL) {
        char *p = strstr(buffer, str1);
        if (p) {
            int index = p - buffer;
            int len1 = strlen(str1);
            int len2 = strlen(str2);

            if (flag == 0) { // Replace all occurrences of str1 with str2
                memmove(p + len2, p + len1, strlen(p + len1) + 1);
                memcpy(p, str2, len2);
                printf("%s", buffer);
            } else if (flag == 1) { // Replace the at-th occurrence of str1 with str2
                count++;
                if (count == at) {
                    memmove(p + len2, p + len1, strlen(p + len1) + 1);
                    memcpy(p, str2, len2);
                    printf("%s", buffer);
                } else {
                    printf("%s", buffer);
                }
            }
        } else {
            printf("%s", buffer);
        }
        // write to file
    }

    fclose(fp);
}

int search_file(char *file_name,char *search_string, int count_flag, int list_flag) {
    FILE *file;
    char buffer[1024];
    int count = 0;

    if (strcmp(file_name, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(file_name, "r");
        if (file == NULL) {
            fprintf(stderr, "Error: Unable to open file %s\n", file_name);
            return 0;
        }
    }

    while (fgets(buffer, sizeof buffer, file)) {
        if (strstr(buffer, search_string) != NULL) {
            if (count_flag) {
                count++;
            } else if (list_flag) {
                return 1;
            } else {
                printf("\n%s : %s",file_name, buffer);
            }
        }
    }

    if (file != stdin) {
        fclose(file);
    }

    return count;
}

void grep(int num_files,int count_flag,int list_flag,char *files[],char *search_string) {
    int total_count = 0;
    if (num_files == 0) {
        total_count = search_file("-", search_string, count_flag, list_flag);
    } else {
        for (int i = 0; i < num_files; i++) {
            total_count += search_file(files[i], search_string, count_flag, list_flag);
        }
    }

    if (count_flag) {
        printf("%d\n", total_count);
    } else if (list_flag) {
        for (int i = 0; i < num_files; i++) {
            if (search_file(files[i],search_string, count_flag, list_flag) > 0) {
                printf("%s\n", files[i]);
            }
        }
    }
}

int undo(const char *file_name) {
  char file_backup[MAX_LEN];
  sprintf(file_backup, "%s.bak", file_name);

  FILE *fp_original = fopen(file_name, "r");
  if (!fp_original) {
    printf("Error: unable to open file %s\n", file_name);
    return 1;
  }

  FILE *fp_backup = fopen(file_backup, "r");
  if (!fp_backup) {
    printf("Error: unable to open file %s\n", file_backup);
    fclose(fp_original);
    return 2;
  }

  char line[MAX_LEN];
  FILE *fp_temp = tmpfile();
  while (fgets(line, MAX_LEN, fp_backup)) {
    fputs(line, fp_temp);
  }

  fclose(fp_original);
  fclose(fp_backup);

  fp_original = fopen(file_name, "w");
  rewind(fp_temp);
  while (fgets(line, MAX_LEN, fp_temp)) {
    fputs(line, fp_original);
  }

  fclose(fp_original);
  fclose(fp_temp);

  remove(file_backup);
  return 0;
}

void auto_indent(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    printf("Error opening file\n");
    return;
  }

  char *line = NULL;
  size_t len = 0;
  int tab_count = 0;

  // Create a temporary file
  FILE *tmp_fp = tmpfile();
  if (!tmp_fp) {
    printf("Error creating temporary file\n");
    fclose(fp);
    return;
  }

  // Read each line of the original file and write it to the temporary file with the desired indentation
  while (getline(&line, &len, fp) != -1) {
    for (int i = 0; i < tab_count; i++) {
      fprintf(tmp_fp, " ");
    }
    for (int i = 0; i < strlen(line); i++) {
      if (line[i] == '{') {
        fprintf(tmp_fp, "{\n");
        tab_count += TAB_SIZE;
        for (int j = 0; j < tab_count; j++) {
          fprintf(tmp_fp, " ");
        }
      } else if (line[i] == '}') {
        tab_count -= TAB_SIZE;
        fprintf(tmp_fp, "\n");
        for (int j = 0; j < tab_count; j++) {
          fprintf(tmp_fp, " ");
        }
        fprintf(tmp_fp, "}");
      } else {
        fprintf(tmp_fp, "%c", line[i]);
      }
    }
  }
  free(line);
  fclose(fp);

  // Reset the file pointers to the start of the files
  fseek(tmp_fp, 0, SEEK_SET);
  fp = fopen(filename, "w");
  if (!fp) {
    printf("Error reopening file for writing\n");
    fclose(tmp_fp);
    return;
  }

  // Copy the content of the temporary file to the original file
  char c;
  while ((c = fgetc(tmp_fp)) != EOF) {
    fputc(c, fp);
  }
  fclose(fp);
  fclose(tmp_fp);
}

void compare(char *file1, char *file2) {
    FILE *fp1, *fp2;
    char buf1[BUFSIZE], buf2[BUFSIZE];
    int line = 1;

    fp1 = fopen(file1, "r");
    if (!fp1) {
        fprintf(stderr, "Error: Could not open file %s\n", file1);
        exit(1);
    }

    fp2 = fopen(file2, "r");
    if (!fp2) {
        fprintf(stderr, "Error: Could not open file %s\n", file2);
        exit(1);
    }

    while (fgets(buf1, BUFSIZE, fp1) && fgets(buf2, BUFSIZE, fp2)) {
        if (strcmp(buf1, buf2) != 0) {
            printf("============ #%d ============\n", line);
            printf("%s", buf1);
            printf("%s", buf2);
        }
        line++;
    }

    // File 1 is longer
    while (fgets(buf1, BUFSIZE, fp1)) {
        printf("<<<<<<<<<<<< #%d - #%d <<<<<<<<<<<<\n", line, line);
        printf("%s", buf1);
        line++;
    }

    // File 2 is longer
    while (fgets(buf2, BUFSIZE, fp2)) {
        printf(">>>>>>>>>>>> #%d - #%d >>>>>>>>>>>>\n", line, line);
        printf("%s", buf2);
        line++;
    }

    fclose(fp1);
    fclose(fp2);
}

void display_tree(char *path, int depth) {
  if (depth < -1) {
    printf("Invalid depth error\n");
    return;
  }

  DIR *dir = opendir(path);
  if (!dir) {
    perror("opendir");
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char next_path[1024];
    snprintf(next_path, sizeof(next_path), "%s/%s", path, entry->d_name);

    for (int i = 0; i < depth; i++) {
      putchar(' ');
    }

    if (entry->d_type == DT_DIR) {
      printf("%s :\n", entry->d_name);
      //if (depth != 0 && (depth != -1)) {
        display_tree(next_path, depth + 1);
     // }
    } else {
      printf("%s\n", entry->d_name);
    }
  }

  closedir(dir);
}


int main(int argc, char *argv[]) {
  char *command = argv[1];
  char *fileName = NULL;
  char *str = NULL;
  char *str1 = NULL;
  char *str2 = NULL;
  int lineNum = -1;
  int startPos = -1;
  int size = -1;
  int at = -1;
  char *direction = NULL;


  // Perform operations based on the given command
    if (strcmp(command, "createFile") == 0) {
      if (argc != 4) {
        printf("Usage: createfile -file <file_name_and_path>\n");
        return 1;
      }
      for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-file") == 0) {
              fileName = argv[++i];
          }
        }
      createFile(fileName);
      createBackup(fileName);
    } else if (strcmp(command, "insertstr") == 0) {
      if (argc != 8) {
        printf("Usage: insertstr <file_name> <str> <line_num> <start_pos>\n");
        return 1;
      }
      for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-file") == 0) {
              fileName = argv[++i];
          }else if(strcmp(argv[i], "-str") == 0){
            str = argv[++i];
          }
          else if(strcmp(argv[i], "-pos") == 0){
            if (sscanf(argv[++i], "%d:%d", &lineNum, &startPos) != 2) {
                  fprintf(stderr, "Invalid value for -pos\n");
                  return 1;
            }
          }
        }
        insertStr(fileName, str, lineNum, startPos);
        createBackup(fileName);
    } else if (strcmp(command, "cat") == 0) {
      if (argc != 4) {
        printf("Usage: cat <file_name>\n", argv[0]);
        return 1;
      }

      for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-file") == 0) {
              fileName = argv[++i];
          }
        }
      cat(fileName);
    } else if (strcmp(command, "removestr") == 0) {
        // Parse the command line arguments
        // print argc
        if (argc != 9) {
            printf("Usage: %s -file <file name> -pos <line no>:<start position> -size <number of characters to remove> -f -b <forward or backward>\n", argv[1]);
            return 1;
        }

        const char *fileName = NULL;
        int line = 0;
        int startPos = 0;
        int size = 0;
        char direction = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-file") == 0) {
                fileName = argv[++i];
            } else if (strcmp(argv[i], "-pos") == 0) {
                if (sscanf(argv[++i], "%d:%d", &line, &startPos) != 2) {
                    fprintf(stderr, "Invalid value for -pos\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-size") == 0) {
                if (sscanf(argv[++i], "%d", &size) != 1) {
                    fprintf(stderr, "Invalid value for -size\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-f") == 0) {
                direction = 'f';
            } else if (strcmp(argv[i], "-b") == 0) {
                direction = 'b';
            } else {
                fprintf(stderr, "Invalid option: %s\n", argv[i]);
                return 1;
            }
        }

      // Call the removeStr function
      removeStr(fileName, line, startPos, size, direction);
    } else if (strcmp(command, "copystr") == 0) {
        if (argc != 9) {
            printf("Usage: %s -file <file name> -pos <line no>:<start position> -size <number of characters to copy> -f -b <forward or backward>\n", argv[1]);
            return 1;
        }

        const char *fileName = NULL;
        int line = 0;
        int startPos = 0;
        int size = 0;
        char direction = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-file") == 0) {
                fileName = argv[++i];
            } else if (strcmp(argv[i], "-pos") == 0) {
                if (sscanf(argv[++i], "%d:%d", &line, &startPos) != 2) {
                    fprintf(stderr, "Invalid value for -pos\n");
                    return 1 ;
                }
                 } else if (strcmp(argv[i], "-size") == 0) {
                if (sscanf(argv[++i], "%d", &size) != 1) {
                    fprintf(stderr, "Invalid value for -size\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-f") == 0) {
                direction = 'f';
            } else if (strcmp(argv[i], "-b") == 0) {
                direction = 'b';
            } else {
                fprintf(stderr, "Invalid option: %s\n", argv[i]);
                return 1;
            }
        }

      // Call the copyStr function
      copyStr(fileName, line, startPos, size, direction);
    } else if(strcmp(command,"cutstr") == 0 ){
        if (argc != 9) {
            printf("Usage: %s -file <file name> -pos <line no>:<start position> -size <number of characters to copy> -f -b <forward or backward>\n", argv[1]);
            return 1;
        }

        const char *fileName = NULL;
        int line = 0;
        int startPos = 0;
        int size = 0;
        char direction = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-file") == 0) {
                fileName = argv[++i];
            } else if (strcmp(argv[i], "-pos") == 0) {
                if (sscanf(argv[++i], "%d:%d", &line, &startPos) != 2) {
                    fprintf(stderr, "Invalid value for -pos\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-size") == 0) {
                if (sscanf(argv[++i], "%d", &size) != 1) {
                    fprintf(stderr, "Invalid value for -size\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-f") == 0) {
                direction = 'f';
            } else if (strcmp(argv[i], "-b") == 0) {
                direction = 'b';
            } else {
                fprintf(stderr, "Invalid option: %s\n", argv[i]);
                return 1;
            }
        }

      // Call the copyStr function
      cutStr(fileName, line, startPos, size, direction);
    } else if (strcmp(command, "pastestr") == 0) {
        if (argc != 6) {
            printf("Usage: %s -file <file name> -pos <line no>:<start position>\n", argv[1]);
            return 1;
        }

        const char *fileName = NULL;
        int line = 0;
        int startPos = 0;
        char direction = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-file") == 0) {
                fileName = argv[++i];
            } else if (strcmp(argv[i], "-pos") == 0) {
                if (sscanf(argv[++i], "%d:%d", &line, &startPos) != 2) {
                    fprintf(stderr, "Invalid value for -pos\n");
                    return 1;
                }
        }else {
                fprintf(stderr, "Invalid option: %s\n", argv[i]);
                return 1;
            }
      }

      // Call the pasteStr function
      pasteStr(fileName, line, startPos);
    } else if (strcmp(command, "find") == 0) {
      int i, count = 0, all = 0, at = -1, byword = 0;
      char *str = NULL, *file = NULL;

      // parse command line arguments
      for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-str")) {
          str = argv[++i];
        } else if (!strcmp(argv[i], "-file")) {
          file = argv[++i];
        } else if (!strcmp(argv[i], "-count")) {
          count = 1;
        } else if (!strcmp(argv[i], "-all")) {
          all = 1;
        } else if (!strcmp(argv[i], "-at")) {
          at = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-byword")) {
          byword = 1;
        }
      }

      // check if required arguments are provided
      if (str == NULL || file == NULL) {
        printf("Error: -str and -file arguments are required\n");
        return 1;
      }

      // check if conflicting options are provided
      if (all && at != -1) {
        printf("Error: -all and -at options cannot be used together\n");
        return 1;
      }

      // open file and read its contents into memory
      FILE *fp = fopen(file, "r");
      if (fp == NULL) {
        printf("Error: Unable to open file %s\n", file);
        return 1;
      }

      char *file_contents = read_file(fp);
      fclose(fp);

      if (file_contents == NULL) {
        printf("Error: Unable to read file %s\n", file);
        return 1;
      }

      // find and print the results
      int *positions = NULL;
      int pos_count = find_str(str, file_contents, &positions, byword);

      if (count) {
        printf("%d\n", pos_count);
      } else if (all) {
        for (i = 0; i < pos_count; i++) {
          printf("%d, ", positions[i]);
        }
        printf("\n");
      } else if (at != -1) {
        if (at > pos_count) {
          printf("-1\n");
        } else {
          printf("%d\n", positions[at - 1]);
        }
      } else if (pos_count > 0) {
        printf("%d\n", positions[0]);
      } else {
        printf("0\n");
      }

      free(file_contents);
      free(positions);
    } else if(strcmp(command,"replace")==0){
      if (argc != 8 && argc != 9) {
        printf("Usage: %s <filename> <str1> <str2> [-at <num> | -all]\n", argv[0]);
        return 1;
      }
      char *filename;
      char *str1;
      char *str2;
      int at = 0;
      int i = 0;
      int all = 0;

      // parse command line arguments
      for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-str1")) {
          str1 = argv[++i];
        }else if (!strcmp(argv[i], "-str2")) {
          str2 = argv[++i];
        } else if (!strcmp(argv[i], "-file")) {
          filename = argv[++i];
        } else if (!strcmp(argv[i], "-all")) {
          all = 1;
        } else if (!strcmp(argv[i], "-at")) {
          at = atoi(argv[++i]);
        }
      }

      if (argc == 9) {
          if (all == 1) {
              replace(filename, str1, str2, 0, 0);
              return 1;
          } else if(at!=0){
              replace(filename, str1, str2, 1, at);
              return 1;
          }else{
              printf("Usage: %s <filename> <str1> <str2> [-at <num> | -all]\n", argv[0]);
              return 1;
          }
      }else{
        replace(filename, str1, str2, 0, 0);
      }
    } else if (strcmp(command, "grep") == 0) {
        if (argc < 6) {
          printf("Usage: %s grep [options] –str <pattern> –files [<file1> <file2> <file3> …]\n", argv[0]);
          return 1;
        }
        int count_flag = 0;
        int list_flag = 0;
        char pattern[MAX_LINE_LEN];
        char *files[MAX_LINE_LEN];
        int num_files = 0;
        for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-c") == 0) {
              count_flag = 1;
          } else if (strcmp(argv[i], "-l") == 0) {
              list_flag = 1;
          } else if (strcmp(argv[i], "-str") == 0) {
              strcpy(pattern, argv[++i]);
          } else if (strcmp(argv[i], "-files") == 0) {
              i++;
              while (i < argc && argv[i][0] != '-') {
                  files[num_files++] = argv[i++];
              }
              i--;
          }
        }
        grep(num_files, count_flag, list_flag,files,pattern);
    } else if (strcmp(command, "auto-indent") == 0) {
        if (argc != 3) {
          printf("Usage: %s auto-indent <file>\n", argv[0]);
          return 1;
        }
        auto_indent(argv[2]);
    } else if (strcmp(command, "tree") == 0) {
      if (argc != 3) {
          fprintf(stderr, "Usage: %s <depth>\n", argv[0]);
          exit(1);
      }
      int depth = 0;
      depth = atoi(argv[1]);

      display_tree(".", depth);
    } else if (strcmp(command, "undo") == 0) {
      if (argc != 3) {
        printf("Usage: %s undo <file>\n", argv[0]);
        return 1;
      }
      return undo(argv[2]);
    } else if (strcmp(command, "compare") == 0) {
        if (argc != 4) {
          fprintf(stderr, "Usage: %s <file1> <file2>\n", argv[0]);
          exit(1);
      }
      compare(argv[2], argv[3]);
    } else{
      printf("Invalid commandii");
    }
  return 0;
}




