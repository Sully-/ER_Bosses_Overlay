#pragma once
#include <windows.h>
#include <string>

namespace er::util {

    void CopyToClipboard(const std::string& text) {
        // Ouvrir le presse-papiers
        if (OpenClipboard(nullptr)) {
            // Vider le presse-papiers actuel
            EmptyClipboard();

            // Allouer de la mémoire pour le texte à copier (global allocation)
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hGlob) {
                // Obtenir un pointeur vers la mémoire allouée
                char* buffer = (char*)GlobalLock(hGlob);
                memcpy(buffer, text.c_str(), text.size() + 1);
                GlobalUnlock(hGlob);

                // Définir le texte dans le presse-papiers
                SetClipboardData(CF_TEXT, hGlob);
            }

            // Fermer le presse-papiers
            CloseClipboard();
        }
    }
}