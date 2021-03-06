#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define IMAGE_READ_BUFFER_SIZE 1024

#define ERR_INVALID_FILE "Error: The source file is not a valid PPM3 or PPM6 file\n"
#define ERR_UNEXPECTED_EOF "Error: Unexpected EOF\n"
#define ERR_OPEN_FILE_READING "Error: Could not open source file for reading '%s'\n"

/**
 * RGB Pixel
 */
typedef struct RGBpixel {
    float r, g, b;
} RGBpixel;

/**
 * Image
 */
typedef struct Image {
    uint32_t width, height;
    RGBpixel* pixmap;
} Image;

/**
 * Show a simple help message about the usage of this program
 */
void show_help() {
    printf("Usage: ezview <input.ppm>\n");
    printf("\t input.ppm: The input image PPM file\n");
    printf("\n");
    printf("\t Example: ezview test.ppm\n");
    printf("\n");
    printf("\t Controls:\n");
    printf("\t\t                WASD - Translation\n");
    printf("\t\t                TFGH - Scale\n");
    printf("\t\t                IJKL - Shear\n");
    printf("\t\t                  QE - Rotation\n");
    printf("\t\t Arrow Up/Arrow Down - Scale uniform\n");
    printf("\t\t      Mouse Scroll Y - Scale uniform by scroll amount\n");
}

/**
 * Increments the pointer past past comments in a PPM file
 * @param fp
 * @return
 */
int skip_comments(FILE* fp) {
    int c;
    char in_comment = FALSE;
    while (TRUE) {
        c = getc(fp);
        if (c == EOF) {
            fprintf(stderr, ERR_INVALID_FILE);
            return 1;
        }
        if (in_comment) {
            if (c == '\n' || c == '\r')
                in_comment = FALSE;
        }
        else if (c == '#')
            in_comment = TRUE;
        else {
            // We read one to far, move back one
            fseek(fp, -1, SEEK_CUR);
            return 0;
        }
    }
};

/**
 * Increments the pointer past whitespace AND comments in a PPM file
 * @param fp
 * @return
 */
int skip_whitespace(FILE* fp) {
    int c;
    do {
        c = getc(fp);
        // make sure we didn't read to the EOF
        if (c == EOF) {
            fprintf(stderr, ERR_UNEXPECTED_EOF);
            return 1;
        }
        if (c == '\n' || c == '\r') {
            // read past any comments
            if (skip_comments(fp) != 0)
                return 1;
        }
    }
    while(c == '\r' || // carriage return
          c == '\n' || // newline
          c == ' '  || // space
          c == '\t'); // tab
    // We read one to far, move back one
    fseek(fp, -1, SEEK_CUR);
    return 0;
}

/**
 * Reads the contents of the current position in the file to the next whitespace character into the specified buffer
 * This function null terminates the buffer at the end of the read string
 * @param fp
 * @param buffer
 * @param buffer_size
 * @return
 */
int read_to_whitespace(FILE* fp, char buffer[], int buffer_size) {
    int c;
    int pos = 0;
    while (TRUE) {
        if (pos > buffer_size - 1) {
            fprintf(stderr, "Error: Buffer not large enough to finish reading to whitespace\n");
            return -1;
        }
        c = getc(fp);
        // make sure we didn't read to the EOF
        if (c == EOF) {
            fprintf(stderr, ERR_UNEXPECTED_EOF);
            return -1;
        }
        if (c == '\r' || // carriage return
            c == '\n' || // newline
            c == ' '  || // space
            c == '\t') { // tab
            fseek(fp, -1, SEEK_CUR);
            // add ASCIIZ, null terminate the string
            buffer[pos] = '\0';
            return pos;
        }
        buffer[pos++] = (char)c;
    }
};

/**
 * Load a PPM P3 file into an image
 * @param fp
 * @param image_ptr
 * @param color_max
 * @param buffer
 * @return
 */
int image_load_p3(FILE* fp, Image* image_ptr, int color_max, char buffer[]) {
    int height = image_ptr->height;
    int width = image_ptr->width;
    // Allocate space for the image in memory
    image_ptr->pixmap = malloc(sizeof(RGBpixel) * width * height);

    // Read the actual image in
    int i;
    int j;
    int k;
    float value;
    int bytes_read;
    for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
            for (k=0; k<3; k++) {
                // Read past the comments and whitespace
                if (skip_whitespace(fp) != 0) {
                    fprintf(stderr, "Error: An error occurred skipping file whitespace\n");
                    return 1;
                }

                bytes_read = read_to_whitespace(fp, buffer, IMAGE_READ_BUFFER_SIZE);
                if (bytes_read <= 0) {
                    fprintf(stderr, "Error: Expected a color value but read nothing\n");
                    return 1;
                }
                else if (atoi(buffer) > color_max) {
                    fprintf(stderr, "Error: A color sample is greater than the maximum color (%i) value \n", color_max);
                    return 1;
                }
                else if (atoi(buffer) < 0) {
                    fprintf(stderr, "Error: A negative color sample is not a valid value \n");
                    return 1;
                }
                value = (atoi(buffer)/(float)color_max);

                if (k == 0)
                    image_ptr->pixmap[i*width + j].r = value;
                if (k == 1)
                    image_ptr->pixmap[i*width + j].g = value;
                if (k == 2)
                    image_ptr->pixmap[i*width + j].b = value;
            }
        }
    }

    return 0;
}

/**
 * Load a PPM P6 file into image_ptr
 * @param fp
 * @param image_ptr
 * @param color_max
 * @param buffer
 * @return
 */
int image_load_p6(FILE* fp, Image* image_ptr, int color_max, char buffer[]) {
    int height = image_ptr->height;
    int width = image_ptr->width;
    image_ptr->pixmap = malloc(sizeof(RGBpixel) * width * height);

    // Read past the comments and whitespace
    if (skip_whitespace(fp) != 0) {
        fprintf(stderr, "Error: An error occurred skipping file whitespace\n");
        return 1;
    }

    // Read the actual image in
    int i;
    int j;
    int k;
    size_t bytes_read;
    float value;
    for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
            for (k=0; k<3; k++) {
                // If color_max is < 256 the values are 8 bit, otherwise they're 16 bit!
                if (color_max < 256) {
                    bytes_read = fread(buffer, sizeof(char), 1, fp);
                    if (bytes_read < 1) {
                        fprintf(stderr, "Error: Expected a color value but read nothing\n");
                        return 1;
                    }
                    value = buffer[0] & 0xFF;
                }
                else {
                    bytes_read = fread(buffer, sizeof(char), 2, fp);
                    if (bytes_read < 2) {
                        fprintf(stderr, "Error: Expected a color value but read nothing\n");
                        return 1;
                    }
                    value = ((buffer[0] << 8) & 0xFF00) | (buffer[1] & 0xFF);
                }

                value = (value/(float)color_max);

                if (k == 0)
                    image_ptr->pixmap[i*width + j].r = value;
                if (k == 1)
                    image_ptr->pixmap[i*width + j].g = value;
                if (k == 2)
                    image_ptr->pixmap[i*width + j].b = value;
            }
        }
    }

    return 0;
}

/**
 * Loads an PPM image in P3 or P6 formats into the specified image_ptr
 * @param image_ptr
 * @param fname
 * @return
 */
int load_image(Image* image_ptr, char* fname) {
    FILE* fp = fopen(fname, "r");
    if (fp) {
        int ppm_version = 0;
        char buffer[IMAGE_READ_BUFFER_SIZE];
        int bytes_read;
        int width;
        int height;
        int color_max;

        bytes_read = read_to_whitespace(fp, buffer, IMAGE_READ_BUFFER_SIZE);

        // Check for the magic number
        if (bytes_read != 2) {
            fprintf(stderr, ERR_INVALID_FILE);
            fclose(fp);
            return 1;
        }
        if (strncmp("P3", buffer, 2) == 0) {
            ppm_version = 3;
        }
        else if (strncmp("P6", buffer, 2) == 0) {
            ppm_version = 6;
        }
        else {
            fprintf(stderr, ERR_INVALID_FILE);
            fclose(fp);
            return 1;
        }

        // Read past the comments and whitespace
        if (skip_whitespace(fp) != 0) {
            fprintf(stderr, "Error: An error occurred skipping file whitespace\n");
            fclose(fp);
            return 1;
        }

        // We are at the first line of image header information
        // Read in the dimensions of the image

        // Read the width of the image
        bytes_read = read_to_whitespace(fp, buffer, IMAGE_READ_BUFFER_SIZE);

        if (bytes_read <= 0) {
            fprintf(stderr, "Error: Expected a width value but read nothing\n");
            return 1;
        }

        width = atoi(buffer);

        if (width < 0)
        {
            fprintf(stderr, ERR_INVALID_FILE);
            fclose(fp);
            return 1;
        }

        // Read past the comments and whitespace
        if (skip_whitespace(fp) != 0) {
            fprintf(stderr, "Error: An error occurred skipping file whitespace\n");
            fclose(fp);
            return 1;
        }

        // Read the height of the image
        bytes_read = read_to_whitespace(fp, buffer, IMAGE_READ_BUFFER_SIZE);

        if (bytes_read <= 0) {
            fprintf(stderr, "Error: Expected a width value but read nothing\n");
            return 1;
        }

        height = atoi(buffer);

        if (height < 0)
        {
            fprintf(stderr, ERR_INVALID_FILE);
            fclose(fp);
            return 1;
        }

        // Read past the comments and whitespace
        if (skip_whitespace(fp) != 0) {
            fprintf(stderr, "Error: An error occurred skipping file whitespace\n");
            fclose(fp);
            return 1;
        }

        // Read in the max color value
        bytes_read = read_to_whitespace(fp, buffer, IMAGE_READ_BUFFER_SIZE);

        if (bytes_read <= 0) {
            fprintf(stderr, "Error: Expected a maximum color value but read nothing\n");
            return 1;
        }

        color_max = atoi(buffer);

        if (color_max < 0 || color_max > 65535)
        {
            fprintf(stderr, "Error: Expected maximum color value between 0 and 65536");
            fclose(fp);
            return 1;
        }

        image_ptr->width = (uint32_t) width;
        image_ptr->height = (uint32_t) height;

        int result;
        if (ppm_version == 6)
            result = image_load_p6(fp, image_ptr, color_max, buffer);

        if (ppm_version == 3)
            result = image_load_p3(fp, image_ptr, color_max, buffer);

        fclose(fp);
        return result;
    }
    else {
        fprintf(stderr, ERR_OPEN_FILE_READING, fname);
        return 1;
    }
}

/**
 * GLFW Window
 */
GLFWwindow* window;

/**
 * Vertex Struct
 */
typedef struct {
    float position[3];
    float color[4];
    float texcords[2];
} Vertex;

/**
 * A simple set of vertieces that define a square with correctly mapped texture cords
 */
const Vertex Vertices[] = {
        {{1, -1, 0}, {1, 1, 1, 1}, {1, 1}},
        {{1, 1, 0}, {1, 1, 1, 1}, {1, 0}},
        {{-1, 1, 0}, {1, 1, 1, 1}, {0, 0}},
        {{-1, -1, 0}, {1, 1, 1, 1}, {0, 1}}
};

/**
 * Specifies the connections between vertices
 */
const GLubyte Indices[] = {
        0, 1, 2,
        2, 3, 0
};

/**
 * The vertex shader for the applciation, this is where almost all the
 * action happens in reguards to transformations of the image. We simply
 * pass values for Scale, Translation, Shear, and Rotation into here and
 * the shader performs the appropriate matrix operations to transform the
 * displayed underlying geometry.
 */
char* vertex_shader_src =
        "attribute vec4 Position;\n"
        "attribute vec4 SourceColor;\n"
        "attribute vec2 SourceTexcoord;\n"
        "uniform vec2 Scale;\n"
        "uniform vec2 Translation;"
        "uniform vec2 Shear;\n"
        "uniform float Rotation;\n"
        "varying vec4 DestinationColor;\n"
        "varying vec2 DestinationTexcoord;\n"
        "mat4 RotationMatrix = mat4( cos(Rotation), -sin(Rotation), 0.0, 0.0,\n"
        "                            sin(Rotation),  cos(Rotation), 0.0, 0.0,\n"
        "                            0.0,            0.0,           1.0, 0.0,\n"
        "                            0.0,            0.0,           0.0, 1.0 );\n"
        "\n"
        "mat4 TranslationMatrix = mat4(1.0, 0.0, 0.0, Translation.x,\n"
        "                              0.0, 1.0, 0.0, Translation.y,\n"
        "                              0.0, 0.0, 1.0, 0.0,\n"
        "                              0.0, 0.0, 0.0, 1.0 );\n"
        "\n"
        "mat4 ScaleMatrix = mat4(Scale.x, 0.0,     0.0, 0.0,\n"
        "                        0.0,     Scale.y, 0.0, 0.0,\n"
        "                        0.0,     0.0,     1.0, 0.0,\n"
        "                        0.0,     0.0,     0.0, 1.0 );\n"
        "\n"
        "mat4 ShearMatrix = mat4(1.0,     Shear.x, 0.0, 0.0,\n"
        "                        Shear.y, 1.0,     0.0, 0.0,\n"
        "                        0.0,     0.0,     1.0, 0.0,\n"
        "                        0.0,     0.0,     0.0, 1.0 );\n"
        "\n"
        "void main(void) {\n"
        "    DestinationColor = SourceColor;\n"
        "    DestinationTexcoord = SourceTexcoord;\n"
        "    gl_Position = Position*ScaleMatrix*ShearMatrix*RotationMatrix*TranslationMatrix;\n"
        "}";


/**
 * The fragment shader for the application. Handles actually mapping the
 * input image onto the geometry.
 */
char* fragment_shader_src =
        "varying vec4 DestinationColor;\n"
        "varying vec2 DestinationTexcoord;\n"
        "uniform sampler2D Texture;\n"
        "\n"
        "void main(void) {\n"
        "    gl_FragColor = texture2D(Texture, DestinationTexcoord) * DestinationColor;\n"
        "}";

/**
 * Compile the specified shader, provides output and checks for errors
 * along the way.
 * @param shader_type - The OpenGL Shader Type
 * @param shader_src - The shader in string form
 * @return The compiled shader id
 */
GLint simple_shader(GLint shader_type, char* shader_src) {
    GLint compile_success = 0;

    // Generate a new shader to work with
    GLuint shader_id = glCreateShader(shader_type);

    // Tell the shader where the source is
    glShaderSource(shader_id, 1, &shader_src, 0);

    // Print the shader before we compile it
    printf("===Compiling Shader===\n");
    printf("%s\n", shader_src);
    printf("======================\n");

    // Actually compile the shader
    glCompileShader(shader_id);

    // Check the status of the compile
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_success);

    // If it failed print an error
    if (compile_success == GL_FALSE) {
        GLchar message[256];
        glGetShaderInfoLog(shader_id, sizeof(message), 0, &message[0]);
        printf("glCompileShader Error: %s\n", message);
        exit(1);
    }

    return shader_id;
}

/**
 * Start the OpenGL program, compile the shaders, and link the program
 * @return
 */
int simple_program() {

    GLint link_success = 0;

    // Generate a new program to work with
    GLint program_id = glCreateProgram();
    // Compile the shaders
    GLint vertex_shader = simple_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLint fragment_shader = simple_shader(GL_FRAGMENT_SHADER, fragment_shader_src);

    // Attach the shaders to the program
    glAttachShader(program_id, vertex_shader);
    glAttachShader(program_id, fragment_shader);

    // Link the program
    glLinkProgram(program_id);

    // Check the link status
    glGetProgramiv(program_id, GL_LINK_STATUS, &link_success);

    // If it failed print an error
    if (link_success == GL_FALSE) {
        GLchar message[256];
        glGetProgramInfoLog(program_id, sizeof(message), 0, &message[0]);
        printf("glLinkProgram Error: %s\n", message);
        exit(1);
    }

    return program_id;
}

/**
 * Print an error that occured in GLFW
 * @param error
 * @param description
 */
static void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

// Define variables to hold our current Scale, Shear, Translation, and Rotation states
float ScaleTo[] = { 1.0, 1.0 };
float Scale[] = { 1.0, 1.0 };
float ShearTo[] = { 0.0, 0.0 };
float Shear[] = { 0.0, 0.0 };
float TranslationTo[] = { 0.0, 0.0 };
float Translation[] = { 0, 0 };
float RotationTo = 0;
float Rotation = 0;

/**
 * The callback called when a key is pressed on the keyboard,
 * this should handle all user input.
 * @param window
 * @param key
 * @param scancode
 * @param action
 * @param mods
 */
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
        switch (key) {
            // Scale up the whole image
            case GLFW_KEY_UP:
                ScaleTo[0] += 0.5;
                ScaleTo[1] += 0.5;
                break;
            // Scale down the whole image
            case GLFW_KEY_DOWN:
                ScaleTo[0] -= 0.5;
                ScaleTo[1] -= 0.5;
                if (ScaleTo[0] < 0)
                    ScaleTo[0] = 0;
                if (ScaleTo[1] < 0)
                    ScaleTo[1] = 0;
                break;
            // Scale up in the Y direction
            case GLFW_KEY_T:
                ScaleTo[1] += 0.5;
                break;
            // Scale down in the Y direction
            case GLFW_KEY_G:
                ScaleTo[1] -= 0.5;
                if (ScaleTo[1] < 0)
                    ScaleTo[1] = 0;
                break;
            // Scale up in the X direction
            case GLFW_KEY_H:
                ScaleTo[0] += 0.5;
                break;
            // Scale down in the X direction
            case GLFW_KEY_F:
                ScaleTo[0] -= 0.5;
                if (ScaleTo[0] < 0)
                    ScaleTo[0] = 0;
                break;
            // Translate down in the X direction
            case GLFW_KEY_A:
                TranslationTo[0] -= 0.5;
                break;
            // Translate up in the X direction
            case GLFW_KEY_D:
                TranslationTo[0] += 0.5;
                break;
            // Translate down in the Y direction
            case GLFW_KEY_S:
                TranslationTo[1] -= 0.5;
                break;
            // Translate up in the Y direction
            case GLFW_KEY_W:
                TranslationTo[1] += 0.5;
                break;
            // Add rotation
            case GLFW_KEY_E:
                RotationTo += 0.1;
                break;
            // Subtract rotation
            case GLFW_KEY_Q:
                RotationTo -= 0.1;
                break;
            // Shear up in the X direction
            case GLFW_KEY_J:
                ShearTo[0] += 0.1;
                break;
            // Shear down in the X direction
            case GLFW_KEY_L:
                ShearTo[0] -= 0.1;
                break;
            // Shear up in the Y direction
            case GLFW_KEY_I:
                ShearTo[1] += 0.1;
                break;
            // Shear down in the Y direction
            case GLFW_KEY_K:
                ShearTo[1] -= 0.1;
                break;
            // Reset all values to their original
            case GLFW_KEY_R:
                ScaleTo[0] = 1.0;
                ScaleTo[1] = 1.0;
                ShearTo[0] = 0.0;
                ShearTo[1] = 0.0;
                TranslationTo[0] = 0.0;
                TranslationTo[1] = 0.0;
                RotationTo = 0;
                break;
        }
}

/**
 * The callback called when the mouse is scrolled
 * @param window
 * @param xoffset
 * @param yoffset
 */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Scale the image by some portion of the amount scrolled in the Y direction
    ScaleTo[0] += yoffset * 0.5;
    ScaleTo[1] += yoffset * 0.5;

    if (ScaleTo[0] < 0)
        ScaleTo[0] = 0;
    if (ScaleTo[1] < 0)
        ScaleTo[1] = 0;
}

/**
 * Tween from a current value to a new value. This function supports arrays of
 * values by specifying how many entries are in the input value sets.
 * @param currentValues The current values that are being tweened
 * @param newValues The new destination values that should be achieved at the end of the tweening
 * @param totalEntries The total number of entries in the input set (currentValues and newValues should have the same length)
 */
void tween(float *currentValues, float *newValues, int totalEntries)
{
    for (totalEntries--; totalEntries >= 0; totalEntries--)
        currentValues[totalEntries] += (newValues[totalEntries] - currentValues[totalEntries]) * 0.1;
}

/**
 * The main enchilada, do all the things!
 */
int main (int argc, char *argv[]) {
    // Check input arguments
    if (argc != 2) {
        fprintf(stderr, "Error: Not enough arguments provided\n");
        show_help();
        return 1;
    }

    // Capture filename to load
    char *inputFname = argv[1];

    // Attempt to load the specified image
    Image image;
    if (load_image(&image, inputFname) != 0) {
        fprintf(stderr, "An error occurred loading the specified source file.\n");
        exit(1);
    }

    // Define GLFW variables
    GLint program_id;
    GLuint color_slot;
    GLuint position_slot;
    GLuint texcoord_slot;
    GLuint scale_slot;
    GLuint translation_slot;
    GLuint rotation_slot;
    GLuint shear_slot;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLuint tex;

    // Set the GLFW error callback
    glfwSetErrorCallback(error_callback);

    // Initialize GLFW library
    if (!glfwInit())
        return -1;

    // Setup GLFW window
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create a fancy window name that has the name of the file being displayed
    char windowName[128];
    snprintf(windowName, sizeof windowName, "ezview - '%s'", inputFname);

    // Create and open a window
    window = glfwCreateWindow(640,
                              480,
                              windowName,
                              NULL,
                              NULL);

    // Make sure the window was created correctly
    if (!window) {
        glfwTerminate();
        printf("glfwCreateWindow Error\n");
        exit(1);
    }

    glfwMakeContextCurrent(window);

    program_id = simple_program();

    glUseProgram(program_id);

    // Configure all the shader slots
    position_slot = glGetAttribLocation(program_id, "Position");
    color_slot = glGetAttribLocation(program_id, "SourceColor");
    texcoord_slot = glGetAttribLocation(program_id, "SourceTexcoord");
    scale_slot = glGetUniformLocation(program_id, "Scale");
    translation_slot = glGetUniformLocation(program_id, "Translation");
    rotation_slot = glGetUniformLocation(program_id, "Rotation");
    shear_slot = glGetUniformLocation(program_id, "Shear");
    glEnableVertexAttribArray(position_slot);
    glEnableVertexAttribArray(color_slot);
    glEnableVertexAttribArray(texcoord_slot);

    // Create Buffer
    glGenBuffers(1, &vertex_buffer);

    // Map GL_ARRAY_BUFFER to this buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

    // Send the data
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices), Indices, GL_STATIC_DRAW);

    int bufferWidth, bufferHeight;
    glfwGetFramebufferSize(window, &bufferWidth, &bufferHeight);

    // Configure the texture
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width, image.height, 0, GL_RGB, GL_FLOAT, image.pixmap);

    glVertexAttribPointer(position_slot,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          0);

    glVertexAttribPointer(color_slot,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (GLvoid*) (sizeof(float) * 3));

    glVertexAttribPointer(texcoord_slot,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (GLvoid*) (sizeof(float) * 7));

    // Setup callbacks for events
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Repeat
    while (!glfwWindowShouldClose(window)) {

        // Tween values
        tween(Scale, ScaleTo, 2);
        tween(Translation, TranslationTo, 2);
        tween(Shear, ShearTo, 2);
        tween(&Rotation, &RotationTo, 1);

        // Send updated values to the shader
        glUniform2f(scale_slot, Scale[0], Scale[1]);
        glUniform2f(translation_slot, Translation[0], Translation[1]);
        glUniform2f(shear_slot, Shear[0], Shear[1]);
        glUniform1f(rotation_slot, Rotation);

        // Clear the screen
        glClearColor(0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0, 0, bufferWidth, bufferHeight);

        // Draw everything
        glDrawElements(GL_TRIANGLES,
                       sizeof(Indices) / sizeof(GLubyte),
                       GL_UNSIGNED_BYTE, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Finished, close everything up
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
