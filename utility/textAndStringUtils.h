

bool isNumberRelatedChar(char c) {
  return (isDigit(c) || (c == '.') || (c == '+') || (c == '-'));
}

int parseNextNumberFromString(String text_buffer, int start_ind, float &value) {
  //find start of number
  while (!isNumberRelatedChar(text_buffer[start_ind])) start_ind++;

  //find end of number
  int end_ind = start_ind;
  while (isNumberRelatedChar(text_buffer[end_ind])) end_ind++;

  //extract number
  value = text_buffer.substring(start_ind, end_ind).toFloat();

  return end_ind;
}
