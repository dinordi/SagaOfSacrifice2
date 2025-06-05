# Use Debian Bullseye for consistent Boost version (1.74)
FROM debian:bullseye AS builder

# Install build dependencies (CMake, g++, make, and Boost 1.74 dev libraries)
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    make \
    libboost-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-chrono-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app
# ... rest of the Dockerfile remains the same ...

# Copy the server source code
COPY server/ .

# Copy the SOS code folder
COPY SOS/ /app/SOS/

ENV DOCKER_BUILD=1

# Create a build directory and build the project
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make

# --- Runtime Stage ---
# Use a smaller base image for the final container
FROM debian:bullseye-slim

# Install runtime dependencies (Boost libraries)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the built executable from the builder stage
COPY --from=builder /app/build/SagaServer .

# Expose the port the server listens on (assuming 12345 from main.cpp)
EXPOSE 8282

# Command to run the server
CMD ["./SagaServer"]