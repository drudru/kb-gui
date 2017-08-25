

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
void show_settings(KBMenu * pmenu);
void key_password(char * pathname);

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
        printf("Cannot connect to kb-gpio process\n");
        sleep(1);
        exit(1);
    }

    srvr.send_msg("kb-gui");
    srvr.recv_ack();

    while (true)
    {
        KBMenu main_menu(&screen, &srvr);

        const char * menu_strs[] = { "Key a password", "Settings", NULL };
        choices.set_list(menu_strs);
        int choice = main_menu.render("KeyBox", &choices, false);

        printf("Chose: %d\n", choice);

        if (choice == 0)
        {
            NXFilePath path{ "/boot/KeyBox/Passwords" };
            show_passwords(&main_menu, &path);
        }
        else
        if (choice == 1)
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

            printf("Passwords Chose: %d\n", choice);
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

            printf("         Choice: %s\n", pdirent->d_name);

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

void show_settings(KBMenu * pmenu)
{
    while (true)
    {
        const char * menu_strs[] = { "About", "Version", "Hardware Test", NULL };
        choices.set_list(menu_strs);
        int choice = pmenu->render("/Settings", &choices, true);

        printf("Settings Chose: %d\n", choice);

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

