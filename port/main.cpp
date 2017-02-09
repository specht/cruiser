#include <GL/glut.h> 
#include <GL/gl.h> 
#include "Gamebuino.h"

extern void setup();
extern void loop();
extern Gamebuino gb;
extern void handle_controls();
extern void move_player();
extern void update_scene();

void key_pressed(unsigned char key, int x, int y)
{
    gb.buttons.key_down[key] = true;
}

void key_released(unsigned char key, int x, int y)
{
    gb.buttons.key_down[key] = false;
}

void display() { 
    glClearColor(1, 1, 1, 0); 
    glClear(GL_COLOR_BUFFER_BIT); 
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glColor3f(0, 0, 0);

    if (gb.update())
    {
        handle_controls();
        move_player();
        update_scene();
    }
    glFlush(); 
    glutPostRedisplay();
} 

int main(int argc, char** argv)
{
    setup();
    glutInit(&argc, argv); 
    glutInitWindowSize(84 * SCREEN_SCALE, 48 * SCREEN_SCALE); 
    glEnable(GL_MULTISAMPLE);
    glutCreateWindow("Cruiser"); 
    glClearColor(1, 1, 1, 0);  
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(-0.5, 83.5, 47.5, -0.5);
    glutDisplayFunc(display); 
    glutKeyboardFunc(key_pressed);
    glutKeyboardUpFunc(key_released);
    glutMainLoop(); 
}
