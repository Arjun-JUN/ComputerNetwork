HTTP Server README

Instructions

    Compilation: gcc serverQ2_cs21b008_cs21b083 -o server -pthread

    Execution:  ./server <port_number> <directory_path>

Features

    ~Handles GET and POST requests.
    ~Multi-threaded for concurrent connections.
    ~Detects MIME types for proper content type headers.
    ~Provides error messages for invalid requests or file not found.

Usage

    ~Ensure directory exists with required files.
    ~Access server via browser or HTTP client.

Example

         Start server on port 8080 serving files from /var/www/html:
bash
         ./server 8080 /var/www/html

Access server at http://localhost:8080.
