#include <glad/glad.h> //OpenGL has different versions on a machhine, with their locations on my graphical driver is unkown, 
//GLAD is loader, when program starts it finds the address of all modern OpenGL functions
#include <GLFW/glfw3.h> // Window manager, creates window, window properties and processing user input

//GLAD needs to be included before GLFW, glad.h contains modern OpenGL function definition that glfw3.h need to see.

#include <stdio.h> // standard input/output library
#include <stdbool.h> // standard boolean library, C doesn't have a built in bool type
#include <math.h>
#include <stb_image.h>
#include <cglm/cglm.h> // Calculator used to calculate the rotation matrix

void processInput(GLFWwindow* window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

int main(){
    if (!glfwInit()){
        printf("Failed to initialoze GLFW\n");
        return -1;
    } // "wakes up" the GLFW library, allocates memory for GLFW internal state
    // CONFIGURE GLFW 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); //We are using version 3.3 of OpenGL, "tutorial uses it", fully supports the core profile.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Gives me the modern OpenGl and disables all the Old functions
    // the other type "GLFW_OPENGL_COMPAT_PROFILE" - gives both modern and old functions
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // "Forward compatible" this even a stricter version of teh CORE_PROFILE
    // it not only disables old functions but disables functions that were marked for removal from 3.3 - required for mac
    //mac doesn't have the old 1990s OpenGL functions at all.


    GLFWwindow* window = glfwCreateWindow(1280, 720, "C Demo Engine", NULL, NULL); //Ask for a 1280x720 window with the title "C.."
    // so GLFWwindow* - regular pointer (8 bytes), glfwCreatewindow creates a big data structure that holds information about our 
    // window, and then returns a memory address. GLFWwindow is an opaque type (struct). internal structure thats hidden. so
    // *window (derefference) C has no idea what that data structure (opaque) looks like, so it will fail
    // glfwCreateWindow accepts 5 arg - int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share. 
    // monitor is for creating a fullscreen. and share is for sharing resources between windows. I wanted a nonsharing, non fullscreen

    //Error handling
    if (window == NULL) { //glfw failed to create a window
        printf("Failed to create GLFW window\n");
        glfwTerminate(); // cleaning up everything glfwInit() did, dont leave a mess
        return -1; // exit program with error code
    }

    //Context
    glfwMakeContextCurrent(window);// tell glfw that the canvas of window is the one we'll draw on

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        printf("Failed to initialize GLAD\n");
        return -1;
    }
    // in simple terms it loads all the modern OpenGL functions, program knows the names from glad.h but not where they are (addresses)
    // glfwGetProcAddress - give this func the name of a func as a string and it will return the address in GLADloadproc type
    // gladLoadGLLoader - "master initializer", func that does the loading, now the glad library now internally hold a "list of addresses"
    // GLADloadproc - typecast, gladLoadGLLoader is very strict, so we are just telling the compilier trust me its the right type


    //Viewport
    //Tell OpenGL the drawing area is the bottom left (0,0) to the top right (1280,720)
    glViewport(0,0,1280,720);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    //when mac os sees window got resized, it sends it to gflw. gflwpollevents() runs and sees this.
    //glfw calls my function and passes it the new width and height it got from the OS

    glEnable(GL_DEPTH_TEST); //pixel by pixel test that garantees triangkes close to the camera are drawn in front of the triangkes further away.
    stbi_set_flip_vertically_on_load(true); //OpenGL's 0,0 is bottom left while images are top left. so we flip vertically


    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture); // Bind the new "wallpaper"
    
    // Set texture wrapping/filtering - in tutorial - need to understand
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // "stbi_load" is our CEO's new tool (from stb_image.h)
    int width, height, nrChannels;
    unsigned char *data = stbi_load("Brick.jpg", &width, &height, &nrChannels, 0);
    
    if (data)
    {
        // "glTexImage2D" is the call to ship the data to the factory (GPU)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Auto-generates smaller versions
    }
    else
    {
        printf("Failed to load texture\n");
    }
    // CEO is done with the raw pixel data, so he frees it
    stbi_image_free(data);


    //Vertex Data
    // Position + color + texture Coords (0,0 bottom left to 1,1 top right)
    // We now have 24 unique vertices (4 for each of the 6 faces)
    // This lets us have perfect control over texture coordinates for each face.
    // Layout: (X,Y,Z), (R,G,B), (S,T) - 8 floats
    float vertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, // Bottom-left
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, // Bottom-right
        0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, // Top-right
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, // Top-left
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, // Bottom-left
        0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f, // Bottom-right
        0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, // Top-right
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f, // Top-left
        // Left face
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f, // Top-right
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, // Top-left
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, // Bottom-left
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, // Bottom-right
        // Right face
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f, // Top-left
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f, // Top-right
        0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f, // Bottom-right
        0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, // Bottom-left
        // Bottom face
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 1.0f, // Top-right
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,  1.0f, 1.0f, // Top-left
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  1.0f, 0.0f, // Bottom-left
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f, // Bottom-right
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f, // Top-left
        0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 1.0f, // Top-right
        0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f, // Bottom-right
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  0.0f, 0.0f  // Bottom-left
    };


    //EBO - Element Buffer object to draw square, better way of drawing, we will replace glDrawArrays with glDrawElements
    // This tells the "Factory" how to pick from the 24 vertices above
    // to build the 12 triangles (36 indices).
    unsigned int indices[] = {
        0, 1, 2,   0, 2, 3,    // Back face
        4, 5, 6,   4, 6, 7,    // Front face
        8, 9, 10,  8, 10, 11,   // Left face
        12, 13, 14, 12, 14, 15,   // Right face
        16, 17, 18, 16, 18, 19,   // Bottom face
        20, 21, 22, 20, 22, 23    // Top face
    };
 
    

    // VBO
    // Vertex Buffer Object - package of data we send to the GPU, vertices we defined live in the RAM. the GPU cant see it 
    // glGenBuffers: Get a unique "tracking number" (an ID) for your package.
    // glBindBuffer: Grab that package (bind it) and put it on the GL_ARRAY_BUFFER "packing table."
    // glBufferData: Stuff your vertices array (the data) into the package.
    unsigned int VBO; // stores address to empty buffer object in GPU memory, its nonnegative so its unsigned
    glGenBuffers(1, &VBO); //create new empty buffer (1) object in GPU memory
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // Binds the buffer object to the binding target (work station) make VBO the currently actve buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // Allocates space on GPU, and transfer vertices to it
    // GL_STATIC_DRAW - 'hint', Static: telling OpenGL we dont intend to change the data often, Draw: data used for drawing
    // basically put this data somewhere fast for the GPU to read, even if its slower for me to write.

    unsigned int EBO;
    glGenBuffers(1, &EBO);
    // Bind and fill EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    
    //The Shaders
    //Vertex Shader - position triangles cornor
    const char* vertexShaderSource = "#version 330 core\n" // since we are using version 3.3 OpenGL, and core disables old functions
        "layout (location = 0) in vec3 aPos;\n" // variable declaration - aPos var name with vec3 data type(vecror 3 float numbers), layout(loctaion=0) address for that input
        //"out vec4 vertexColor;\n" // Declare an out variable that fragment shader can get
        "layout (location = 1) in vec3 aColor;\n"
        "layout (location = 2) in vec2 aTextCoord;\n"
        "uniform mat4 model;\n" // global variable, mat4 is a data type (4x4matrix). model is the variable name
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec3 ourColor;\n"
        "out vec2 TextCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = projection* view * model * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n" //built in var that takes our aPos and turns into vec4, by adding 1.0 as w
        //"   vertexColor = vec4(1.0,0.5,0.2,1.0);"
        "   ourColor = aColor;\n"
        "   TextCoord = aTextCoord;\n"
        "}\0";
    //Fragment Shader -  run for every single pixel and decide its color
    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n" // Output, shader will output a vec4
        //"in vec4 vertexColor;\n" // declare in variable name and type must match vertex shader
        "in vec3 ourColor;\n"
        "in vec2 TextCoord;\n"
        "uniform sampler2D ourTexture;\n" // wallpaper tool
        //"uniform vec4 ourColor;\n" // a global variable for all "worker", we can send data every frame 
        "void main()\n"
        "{\n"
        //"   FragColor = vertexColor;" // Orange
        "   FragColor = texture(ourTexture, TextCoord);\n " // * vec4(ourColor, 1.0);\n"
        "}\n\0";
    //Create and compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER); //Create new "shader object"
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); // Attach my source code string to the object. 2nd arguement is the no. strings being passed as source code.
    glCompileShader(vertexShader); // Compile the shader

    //Error Checking
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    //Error Checking
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
    }

    //Link the shaders into a shaders program
    unsigned int shaderProgram = glCreateProgram();
    //Attach two shaders together
    glAttachShader(shaderProgram, vertexShader); 
    glAttachShader(shaderProgram, fragmentShader);
    //Link them together
    glLinkProgram(shaderProgram);

    
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);//can now delete the individual shaders; they're "baked into" the program

    // int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor"); // The C code sends the ID of the compiled shader programs\
    to the graphics driver. also sends the string "ourColor". Driver performs a lookup inside its internal tables for shaderProgram\
    it searches for the variable name "ourColor", if found it returns an int, if not returns -1. glGetUniformLocation is a slow one time\
    lookup to get a fast permanent address, that we can use every frame in the while loop. use if statement to check not found
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model"); //integer location for transform "phone extension"  
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");  
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    // checks if the "phone extensions" are valid.
    if (modelLoc == -1)
    {
        printf("ERROR::SHADER::UNIFORM_NOT_FOUND [model]\n");
    }
    if (viewLoc == -1)
    {
        printf("ERROR::SHADER::UNIFORM_NOT_FOUND [view]\n");
    }
    if (projectionLoc == -1)
    {
        printf("ERROR::SHADER::UNIFORM_NOT_FOUND [projection]\n");
    }



    //VAO  - instruction manual. we bind it open, write all the instructions and then unbind it.
    unsigned int VAO; 
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO); //Open the instruction manual
    glBindBuffer(GL_ARRAY_BUFFER, VBO); //Bind our VBO (the data package)
    // glVertexAttribPointer(0,3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); // Write the instruction for location 0
    //This instruction is for location 0, 3,GL_FLOAT: The data is 3 GL_FLOAT's. 3 * sizeof(float), (void*)0: This data is found\
    at the beginning of the VBO ((void*)0 -nits an offset), and each new vertex starts 3 floats after the lasts.
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);


    //Page 1: Position (location 0)
    glVertexAttribPointer(0,3,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); //Turn on location 0

    //Page 2: Color (location 1)
    glVertexAttribPointer(1,3,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3*sizeof(float))); // stride is 8 floats, offset is 3 floats.
    glEnableVertexAttribArray(1); //Turn on location 1

    //Page 3: TexCoord (location 2)
    glVertexAttribPointer(2,2,GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2); //Turn on location 2


    glBindVertexArray(0); // Close VAO (the instruction manual).

    // Define 10 cube positions for our "World"
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


    //Render Loop
    while (!glfwWindowShouldClose(window)){ //keep looping as long as window has not been told to close
        //Each time the loop runs is one frame

        processInput(window); // a function we will add later, to check for keyboard presses, asks glfwPollEvents about the state
        // querying the state of a keyboard key. e.g. has the Escape key changed to a pressed state?
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // set "clear" color to dark teal
        //OpenGL is a giant state machine and that was a state setting function, OpenGL clear color setting is now dark teal.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //action function, looks at the clear color setting and uses that color to paint the screen
        // GL_COLOR_BUFFET_BIT - a flag (a bit), basically "which wall". modern graphics canvas are made up of multiple layers. 
        // Color buffer: main layer, red blue green pixels. - so we tell glClear on clear this layer 
        // depth buffer: stores how far each pixel is (3D). Stencil buffer: Another hidden layer for advanced masking effects.
        
        //Bind wallpaper before drawing
        glActiveTexture(GL_TEXTURE0); //activate texture unit 0
        glBindTexture(GL_TEXTURE_2D, texture); //Bind our texture to unit 0

        glUseProgram(shaderProgram); //Activate our compiled shader program

        //mat4 transform = GLM_MAT4_IDENTITY_INIT;// Create empty identity matrix
        // 3. "CEO" builds the transformation
        // NOTE: The order is "backwards" from the PDF.
        // We want to scale, THEN rotate, THEN translate.
        // CGLM's functions "pre-multiply", so you call them in the *reverse* order.
        
        // A. Translate the (already rotated) square to the corner
        //glm_translate(transform, (vec3){ 0.5f, -0.5f, 0.0f }); 
        
        // B. Rotate it based on time
        //glm_rotate(transform, (float)glfwGetTime(), (vec3){ 0.0f, 0.0f, 1.0f });

        // C. Scale it down first
        //glm_scale(transform, (vec3){ 0.5f, 0.5f, 0.5f });
        //Final = Translate * rotate * scale 

        // 4. "CEO" makes the "PA Announcement"
        // Send the final 'transform' matrix to the "phone extension"
        //glUniformMatrix4fv(transformLoc, 1, GL_FALSE, (float*)transform);// transformLoc - phone extension
        // 1 - no. matrix, GL_FALSE - dont transpose, (float*)transform - the actual data

        // float timeValue = glfwGetTime(); // calculates how much time has passed since we initialized the glfw
        // float greenValue = (sin(timeValue)/2.0f) + 0.5f; //normalized sin wave that oscillates 0.0 - 1.0
        // glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f); // broadcast the color

        glBindVertexArray(VAO); // Open my instruction manual

        // 1. PROJECTION (The "Lens")
        // We create this *once per frame*. (Could be outside the loop if you don't resize)
        mat4 projection = GLM_MAT4_IDENTITY_INIT;
        // Creates a 45-degree Field-of-View, 1280/720 aspect ratio, 0.1 near, 100 far
        glm_perspective(glm_rad(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f, projection);
        // "PA Announcement" for the "Lens"
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (float*)projection);

        // 2. VIEW (The "Security Camera")
        mat4 view = GLM_MAT4_IDENTITY_INIT;
        // "CEO wants to move back 3 units" -> "Move the whole world forward 3 units"
        glm_translate(view, (vec3){ 0.0f, 0.0f, -3.0f });
        // "PA Announcement" for the "Camera"
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (float*)view);

        // 3. MODEL (The "Object Blueprints")
        // This is the payoff! We loop 10 times and draw 10 cubes.
        for(int i = 0; i < 10; i++)
        {
            // "CEO" starts with a fresh "Blueprint" for this object
            mat4 model = GLM_MAT4_IDENTITY_INIT;
            
            // "Blueprint step 1: Put it at its world position"
            glm_translate(model, cubePositions[i]);
            
            // "Blueprint step 2: Give it a unique rotation"
            float angle = 20.0f * i + (float)glfwGetTime() * 25.0f;
            glm_rotate(model, glm_rad(angle), (vec3){ 1.0f, 0.3f, 0.5f });
            
            // "PA Announcement" for THIS object's "Blueprint"
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);
            // [CHANGED!] We are now drawing 36 *indices* from the EBO
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        //glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Starting index 0, no. vertices - 4, primitive - GL_TRIANGLE_STRIP 
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); //Draw 6 indices
        
        
        glfwSwapBuffers(window); // swap buffers
        glfwPollEvents(); // goes to the OS and collects all new events and puts them in a queue (including window resizing). 
        //called every frame (60 a sec).

    }
    glfwTerminate();
    return 0;
}
void processInput(GLFWwindow* window){
    //check if 'Escape' key has been pressed
    if (glfwGetKey(window, GLFW_KEY_ESCAPE)==GLFW_PRESS){
        // If so close the window
        glfwSetWindowShouldClose(window, true); // uses true from stdbool.h
    }
    // glfwGetKey is how you poll for a key, you give it the window address and the key you are looking for.
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    glViewport(0,0,width,height);
}
