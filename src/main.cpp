

#include "common.h"

#include "NXGeom.hpp"
#include "NXFilePath.hpp"
#include "NXFileDir.hpp"
#include "NXMMapFile.hpp"
#include "KBMenu.hpp"

#include "KBConstStringList.hpp"

#include <sys/types.h>
#include <sys/wait.h>

void show_passwords(KBMenu * pmenu, NXFilePath * ppath);
void show_genpass(KBMenu * pmenu);
void show_settings(KBMenu * pmenu);
void key_password(char * pathname);
void gen_password();
void gen_rand(const char * base);

KBConstStringList choices;

main()
{
    // Run Loop
    //

    // Render to /dev/fb1
    //
    int fbfd = open("/dev/fb1", O_RDWR);

    NXRect screen_rect = {0, 0, 320, 240};
    int screen_datasize = screen_rect.size.w * screen_rect.size.h * 2;
    void * fbp = mmap(0, screen_datasize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    close(fbfd);
    NXCanvas screen { NXBitmap{(uint8_t *)fbp, screen_rect, NXColorChan::RGB565} }; 

    /*
    // Blit
    NXBlit::blit(&font_bmp,   &screen_rect,    // Copy font-rect
                 &screen_bmp, &screen_rect);
    //NXBlit::blit(&font_bmp,   &font_bmp.rect,    // Copy font-rect
    //             &screen_bmp, &font_bmp.rect);
    */

    //stbi_image_free( font );
    //
    // Connect to Unix domain socket for events
    auto srvr = NXUnixPacketSocket::CreateClient("/tmp/kb-gpio");

    if (!srvr.valid())
    {
        fprintf(stderr, "Cannot connect to kb-gpio process\n");
        sleep(1);
        exit(1);
    }

    srvr.send_msg("kb-gui");
    srvr.recv_ack();

    while (true)
    {
        KBMenu main_menu(&screen, &srvr);

        const char * menu_strs[] = {
            "Key a password",
            "Gen a password",
            "Settings",
            NULL
        };
        choices.set_list(menu_strs);
        int choice = main_menu.render("KeyBox", &choices, false);

        fprintf(stderr, "Chose: %d\n", choice);

        if (choice == 0)
        {
            NXFilePath path{ "/boot/KeyBox/Passwords" };
            show_passwords(&main_menu, &path);
        }
        else
        if (choice == 1)
        {
            show_genpass(&main_menu);
        }
        else
        if (choice == 2)
            show_settings(&main_menu);
    }

}

void show_passwords(KBMenu * pmenu, NXFilePath * ppath)
{
    while (true)
    {

        {
            NXFileDir dir(ppath);
            NXHumanDir choices(&dir);

            // TODO Sprintf title

            int choice = pmenu->render(ppath->basename(), &choices, true);

            fprintf(stderr, "Passwords Chose: %d\n", choice);
            if (choice == -1)
                return;

            choices.rewind();

            (void) choices.get_next();
            struct dirent * pdirent = choices.get_dirent();

            int tmp = choice;
            while (tmp--)
            {
                (void) choices.get_next();
                pdirent = choices.get_dirent();
            }

            fprintf(stderr, "         Choice: %s\n", pdirent->d_name);

            if (pdirent->d_type == DT_REG)
            {
                NXFilePath new_path = ppath->add(pdirent->d_name);
                pmenu->display_status("Run...");
                key_password(new_path.path());
            }
            else
            {
                NXFilePath new_path = ppath->add(pdirent->d_name);
                pmenu->display_status("Open...");
                show_passwords(pmenu, &new_path);
            }
        }
    }
}

void show_genpass(KBMenu * pmenu)
{
    while (true)
    {
        const char * menu_strs[] = { "Hexadecimal", "Base 36", NULL };
        choices.set_list(menu_strs);
        int choice = pmenu->render("/GenPass", &choices, true);

        fprintf(stderr, "Settings Chose: %d\n", choice);

        if (choice == -1)
            return;
        else
        if (choice == 0)
        {
            pmenu->display_status("Gen Hex...");
            gen_rand("16");
        }
        else
        if (choice == 1)
        {
            pmenu->display_status("Gen Base 36...");
            gen_password();
        }
    }
}

void show_settings(KBMenu * pmenu)
{
    while (true)
    {
        const char * menu_strs[] = { "About", "Version", "Hardware Test", NULL };
        choices.set_list(menu_strs);
        int choice = pmenu->render("/Settings", &choices, true);

        fprintf(stderr, "Settings Chose: %d\n", choice);

        if (choice == -1)
            return;
    }
}

void key_password(char * pathname)
{
    NXMmapFile fmap;
    if (!fmap.map(pathname))
        return;

    if (fmap.ptr()[0] != '#')
    {
        // Send the keys
        //
        int pid = fork();

        if (pid < 0)
            panic();

        if (pid == 0)
        {
            // Child
            char * const newenviron[] = { NULL };

            const char * tmpargv[]    = { "./kb-key", "/dev/hidg0", NULL, NULL };
            tmpargv[2] = fmap.ptr();

            char * const * newargv = (char * const *)tmpargv;
            execve(newargv[0], newargv, newenviron);
            perror("execve");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Parent
            int status;
            int res = wait(&status);

            if ((res == -1) || (res != pid))
                panic();
        }
    }

}

void gen_password()
{
    // Gen the password
    //
    //

    int pfd[2];
    char buff[9];

    if (pipe(pfd) == -1)
        panic();

    int pid = fork();
    if (pid < 0)
        panic();

    if (pid == 0)
    {
        // Child

        // close read-end
        close(pfd[0]);

        if (dup2(pfd[1], 1) == -1)  // pipe write end -> stdout
            panic();
        close(pfd[1]);

        char * const newenviron[] = { NULL };

        const char * tmpargv[]    = { "./kb-genpass", "8", NULL };

        char * const * newargv = (char * const *)tmpargv;
        execve(newargv[0], newargv, newenviron);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent

        // close write-end
        close(pfd[1]);

        if (read(pfd[0], buff, 9) != 9)
            panic();
        buff[8] = 0; // cut-off the newline

        int status;
        int res = wait(&status);

        if ((res == -1) || (res != pid))
            panic();

        close(pfd[0]);
    }

    // Send the keys

    pid = fork();

    if (pid < 0)
        panic();

    if (pid == 0)
    {
        // Child
        char * const newenviron[] = { NULL };

        const char * tmpargv[]    = { "./kb-key", "/dev/hidg0", NULL, NULL };
        tmpargv[2] = buff;

        char * const * newargv = (char * const *)tmpargv;
        execve(newargv[0], newargv, newenviron);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent
        int status;
        int res = wait(&status);

        if ((res == -1) || (res != pid))
            panic();
    }

}

void gen_rand(const char * base)
{
    // TODO: validate base

    int pfd[2];
    char buff[9];

    if (pipe(pfd) == -1)
        panic();

    int pid = fork();
    if (pid < 0)
        panic();

    if (pid == 0)
    {
        // Child

        // close read-end
        close(pfd[0]);

        if (dup2(pfd[1], 1) == -1)  // pipe write end -> stdout
            panic();
        close(pfd[1]);

        char * const newenviron[] = { NULL };

        const char * tmpargv[]    = { "./kb-genrand", NULL, "8", NULL };
        tmpargv[1] = base;

        char * const * newargv = (char * const *)tmpargv;
        execve(newargv[0], newargv, newenviron);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent

        // close write-end
        close(pfd[1]);

        if (read(pfd[0], buff, 9) != 9)
            panic();
        buff[8] = 0; // cut-off the newline

        int status;
        int res = wait(&status);

        if ((res == -1) || (res != pid))
            panic();

        close(pfd[0]);
    }

    // Send the keys

    pid = fork();

    if (pid < 0)
        panic();

    if (pid == 0)
    {
        // Child
        char * const newenviron[] = { NULL };

        const char * tmpargv[]    = { "./kb-key", "/dev/hidg0", NULL, NULL };
        tmpargv[2] = buff;

        char * const * newargv = (char * const *)tmpargv;
        execve(newargv[0], newargv, newenviron);
        perror("execve");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent
        int status;
        int res = wait(&status);

        if ((res == -1) || (res != pid))
            panic();
    }
}

