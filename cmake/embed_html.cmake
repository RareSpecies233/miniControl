# cmake/embed_html.cmake
# Reads a file and embeds it as a C++ raw string literal
file(READ "${INPUT}" content)
file(WRITE "${OUTPUT}" "#pragma once\n\nstatic const char* HTML_CONTENT = R\"rawliteral(${content})rawliteral\";\n")
