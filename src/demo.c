#include <glad/glad.h>
#include <GLFW/glfw3.h> 

// --- AUDIO INCLUDES (MacOS) ---
// We need these specifically for the AudioQueue API
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
// ------------------------------

#include <stdio.h> 
#include <stdbool.h> 
#include <math.h>
#include <stb_image.h>
#include <cglm/cglm.h> 
#include <pthread.h>
#include <mach/mach_time.h> 

// Global mutex to protect audio state between Main Thread and Audio Thread
static pthread_mutex_t g_mutex;

// --- AUDIO GLOBALS & FUNK ENGINE ---
// Holds the state of our FM synthesizer
typedef struct { 
    double phase; 
    double freq; 
    double vol; 
    int samples_left; 
    double mod_phase; // Phase for the FM modulator (the "funk" texture)
} AudioState;

static AudioQueueRef g_q = NULL; 
static AudioQueueBufferRef g_bufs[3]; 
static AudioState g_as;

// --- THE AUDIO CALLBACK ---
// This runs on a separate high-priority OS thread.
// It asks us to fill a buffer with PCM data.
static void AQCallback(void *ud, AudioQueueRef q, AudioQueueBufferRef buf){
    (void)ud; 
    const double sr = 44100.0; 
    int16_t *out = (int16_t*)buf->mAudioData;
    int N = (int)buf->mAudioDataBytesCapacity / 2; 

    // [Concurrency Check] 
    // We MUST lock here. If the main thread changes the note frequency while 
    // we are halfway through calculating this buffer, the waveform will snap,
    // causing a nasty "pop".
    pthread_mutex_lock(&g_mutex);
    
    // [Performance Check] 
    // Measure how long the synthesis math takes using low-level Mach timers
    uint64_t start = mach_absolute_time();
    
    // Carrier frequency setup
    double step = (2.0 * M_PI * g_as.freq) / sr;
    
    // Modulator setup (2.0 ratio gives a harmonic/square-ish tone)
    double mod_step = step * 2.0; 

    for(int i = 0; i < N; i++){
        if(g_as.samples_left > 0){
            // 1. Envelope Generator
            // Simple attack/decay for a percussive "slap bass" feel
            int tot = (int)(0.25 * sr); 
            int age = tot - g_as.samples_left;
            double env = 1.0;
            
            if(age < 100) env = (double)age / 100.0; // Fast attack
            else env = exp(-15.0 * ((double)(age - 100) / sr)); // Exp decay

            // 2. Advance Phases
            g_as.phase += step; 
            g_as.mod_phase += mod_step;
            
            // Wrap phases to keep precision happy
            if (g_as.phase > 2.0 * M_PI) g_as.phase -= 2.0 * M_PI;
            if (g_as.mod_phase > 2.0 * M_PI) g_as.mod_phase -= 2.0 * M_PI;

            // 3. FM Synthesis
            // Modulate the carrier's phase with the modulator's amplitude
            double modulation = sin(g_as.mod_phase) * 3.0 * env; 
            double raw_wave = sin(g_as.phase + modulation);

            // 4. Hard Clip / Distortion
            // Keeps it loud and gritty
            if (raw_wave > 0.8) raw_wave = 0.8;
            if (raw_wave < -0.8) raw_wave = -0.8;

            // Output 16-bit signed integer
            out[i] = (int16_t)(raw_wave * 32767.0 * g_as.vol * env);
            g_as.samples_left--;
        } else { 
            // Silence if no note is playing
            out[i] = 0; 
        }
    }

    // Stop the performance timer
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    // Done modifying shared state
    pthread_mutex_unlock(&g_mutex);

    // Tell the OS how many bytes we wrote
    buf->mAudioDataByteSize = (UInt32)(N * 2); 

    // Occasionally print the synth performance stats
    // (Check this in terminal to prove we aren't blocking the thread too long)
    static int count = 0;
    if (count++ > 100){
        printf("Synth took %llu mach ticks\n", elapsed);
        count = 0; 
    }

    // Hand the buffer back to the OS to play
    AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

// Setup the Mac AudioQueue system
static int audio_init(void){
    pthread_mutex_init(&g_mutex, NULL);
    memset(&g_as, 0, sizeof(g_as)); 
    g_as.freq = 55.0; // Start at A1
    g_as.vol = 0.5; 
    g_as.samples_left = 0;
    
    // Define standard CD-quality audio format (16-bit PCM)
    AudioStreamBasicDescription asbd = {0};
    asbd.mSampleRate = 44100.0; 
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    asbd.mBitsPerChannel = 16; 
    asbd.mChannelsPerFrame = 1; // Mono
    asbd.mBytesPerFrame = 2;
    asbd.mFramesPerPacket = 1; 
    asbd.mBytesPerPacket = 2;

    if (AudioQueueNewOutput(&asbd, AQCallback, NULL, NULL, NULL, 0, &g_q) != noErr || !g_q) return 0;
    
    // Allocate 3 buffers. This triple-buffering ensures smooth playback.
    const UInt32 BYTES = 1024 * 2; 
    for(int i = 0; i < 3; i++){ 
        AudioQueueAllocateBuffer(g_q, BYTES, &g_bufs[i]); 
        g_bufs[i]->mAudioDataByteSize = BYTES; 
        memset(g_bufs[i]->mAudioData, 0, BYTES); 
        // Prime the queue by enqueueing silent buffers first
        AudioQueueEnqueueBuffer(g_q, g_bufs[i], 0, NULL); 
    }
    return AudioQueueStart(g_q, NULL) == noErr;
}

// Trigger a new note (Producer)
static void audio_slap(double freq){ 
    // [Concurrency Check]
    // Lock before writing to shared frequency/phase state
    pthread_mutex_lock(&g_mutex);
    g_as.freq = freq; 
    g_as.phase = 0; // Reset phase for consistent attack
    g_as.mod_phase = 0;
    g_as.samples_left = (int)(0.25 * 44100.0); 
    pthread_mutex_unlock(&g_mutex);
}

static void audio_shutdown(void){ 
    if(g_q){ AudioQueueStop(g_q, true); AudioQueueDispose(g_q, true); g_q = NULL; } 
}

// Generate frequencies for a Minor Pentatonic scale
static double get_funky_bass_note(int k){ 
    static const int st[] = {0, 3, 5, 7, 10}; 
    int scale_idx = k % 5;
    int octave = (k / 5) % 2; 
    double base = 55.0 * pow(2.0, octave); 
    return base * pow(2.0, st[scale_idx] / 12.0); 
}

void processInput(GLFWwindow* window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

int main(){
    // 1. Initialize Audio System
    if (!audio_init()) { printf("Audio Init Failed\n"); return -1; }

    // 2. Initialize Windowing System (GLFW)
    if (!glfwInit()){
        printf("Failed to initialoze GLFW\n");
        return -1;
    } 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

    GLFWwindow* window = glfwCreateWindow(1280, 720, "C Demo Engine", NULL, NULL); 

    if (window == NULL) { 
        printf("Failed to create GLFW window\n");
        glfwTerminate(); 
        return -1; 
    }

    glfwMakeContextCurrent(window);

    // 3. Initialize OpenGL Loader (GLAD)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    //glViewport(0,0,1280,720);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glEnable(GL_DEPTH_TEST); 
    
    // Flip textures because OpenGL expects 0.0 on Y axis at bottom
    stbi_set_flip_vertically_on_load(true); 

    // Load and set up texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture); 
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char *data = stbi_load("Brick.jpg", &width, &height, &nrChannels, 0);
    
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); 
    } else {
        printf("Failed to load texture\n");
    }
    stbi_image_free(data);

    // Cube Vertex Data
    float vertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 
        0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 
        0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f, 
        0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 
        // Left face
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 
        // Right face
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f, 
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 
        0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 
        0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 
        // Bottom face
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 1.0f, 
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  1.0f, 0.0f, 
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 
        0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 1.0f, 
        0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  0.0f, 0.0f  
    };

    unsigned int indices[] = {
        0, 1, 2,   0, 2, 3,    
        4, 5, 6,   4, 6, 7,    
        8, 9, 10,  8, 10, 11,   
        12, 13, 14, 12, 14, 15,   
        16, 17, 18, 16, 18, 19,   
        20, 21, 22, 20, 22, 23    
    };

    // Setup VBO, EBO, VAO
    unsigned int VBO, EBO, VAO;
    glGenBuffers(1, &VBO); 
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO); 

    glBindBuffer(GL_ARRAY_BUFFER, VBO); 
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); 

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Vertex Position
    glVertexAttribPointer(0,3,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); 
    // Vertex Color
    glVertexAttribPointer(1,3,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3*sizeof(float))); 
    glEnableVertexAttribArray(1); 
    // Texture Coords
    glVertexAttribPointer(2,2,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2); 

    glBindVertexArray(0); 

    // --- SHADER SETUP ---
    const char* vertexShaderSource = "#version 330 core\n" 
        "layout (location = 0) in vec3 aPos;\n" 
        "layout (location = 1) in vec3 aColor;\n"
        "layout (location = 2) in vec2 aTextCoord;\n"
        "uniform mat4 model;\n" 
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec3 ourColor;\n"
        "out vec2 TextCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = projection* view * model * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n" 
        "   ourColor = aColor;\n"
        "   TextCoord = aTextCoord;\n"
        "}\0";

    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n" 
        "in vec3 ourColor;\n"
        "in vec2 TextCoord;\n"
        "uniform sampler2D ourTexture;\n" 
        "void main()\n"
        "{\n"
        "   FragColor = texture(ourTexture, TextCoord);\n " 
        "}\n\0";
    
    // Compile Vertex Shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER); 
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); 
    glCompileShader(vertexShader); 

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
    }

    // Compile Fragment Shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
    }

    // Link Program
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); 
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Locate Uniforms
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model"); 
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");  
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    // Positions for the spinning cubes
    vec3 cubePositions[] = {
        { 0.0f,  0.0f,   0.0f},
        { 2.0f,  5.0f, -15.0f},
        {-1.5f, -2.2f,  -2.5f},
        {-3.8f, -2.0f, -12.3f},
        { 2.4f, -0.4f,  -3.5f},
        {-1.7f,  3.0f,  -7.5f},
        { 1.3f, -2.0f,  -2.5f},
        { 1.5f,  2.0f,  -2.5f},
        { 1.5f,  0.2f,  -1.5f},
        {-1.3f,  1.0f,  -1.5f}
    };

    int last_beat_tick = -1;

    // --- MAIN RENDER LOOP ---
    while (!glfwWindowShouldClose(window)){ 
        
        // Check global time
        double time = glfwGetTime();
        
        // Required: Quit automatically after 60 seconds for the demo
        if (time > 60.0){
            glfwSetWindowShouldClose(window, true);
        }

        // Simple "Beat" calculator (approx 480 BPM 16th notes for fast funk)
        int current_beat_tick = (int)(time * 8.0); 
        
        if (current_beat_tick != last_beat_tick) {
            last_beat_tick = current_beat_tick;
            // Trigger a random note from the Pentatonic Scale
            if (rand() % 10 > 2) { // 80% chance to play
                 double note = get_funky_bass_note(rand() % 15);
                 audio_slap(note); // Safe producer call
            }
        }

        processInput(window); 

        // Clear Screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
        
        // Bind Texture
        glActiveTexture(GL_TEXTURE0); 
        glBindTexture(GL_TEXTURE_2D, texture); 
        glUseProgram(shaderProgram); 
        glBindVertexArray(VAO); 

        // Camera / View Matrices
        mat4 projection = GLM_MAT4_IDENTITY_INIT;
        glm_perspective(glm_rad(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f, projection);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (float*)projection);

        mat4 view = GLM_MAT4_IDENTITY_INIT;
        glm_translate(view, (vec3){ 0.0f, 0.0f, -3.0f });
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (float*)view);

        // Draw all 10 cubes
        for(int i = 0; i < 10; i++)
        {
            mat4 model = GLM_MAT4_IDENTITY_INIT;
            glm_translate(model, cubePositions[i]);
            // Rotate based on time and index
            float angle = 20.0f * i + (float)glfwGetTime() * 25.0f;
            glm_rotate(model, glm_rad(angle), (vec3){ 1.0f, 0.3f, 0.5f });
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);
            
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        
        glfwSwapBuffers(window); 
        glfwPollEvents(); 
    }
    // cleanup
    audio_shutdown();
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window){
    if (glfwGetKey(window, GLFW_KEY_ESCAPE)==GLFW_PRESS){
        glfwSetWindowShouldClose(window, true); 
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    glViewport(0,0,width,height);
}