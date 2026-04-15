from PIL import Image, ImageDraw

# Résolution stricte de l'écran ILI9341 en mode paysage
WIDTH = 320
HEIGHT = 240

def create_face(filename, emotion):
    # Créer une image noire RGB
    img = Image.new('RGB', (WIDTH, HEIGHT), color=(0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Couleur des traits du robot (Bleu Cyan / Robotique)
    color = (0, 255, 255)

    # Yeux (communs à toutes les émotions)
    eye_width, eye_height = 40, 60
    left_eye_pos = [80, 60, 80 + eye_width, 60 + eye_height]
    right_eye_pos = [200, 60, 200 + eye_width, 60 + eye_height]

    if emotion == "neutre":
        # Yeux normaux
        draw.rectangle(left_eye_pos, fill=color)
        draw.rectangle(right_eye_pos, fill=color)
        # Bouche droite
        draw.line([100, 180, 220, 180], fill=color, width=10)

    elif emotion == "joie":
        # Yeux souriants (arcs)
        draw.arc([70, 40, 130, 100], start=180, end=360, fill=color, width=15)
        draw.arc([190, 40, 250, 100], start=180, end=360, fill=color, width=15)
        # Bouche souriante
        draw.arc([100, 120, 220, 200], start=0, end=180, fill=color, width=15)

    elif emotion == "triste":
        # Yeux tristes
        draw.rectangle([80, 80, 80 + eye_width, 80 + eye_height], fill=color)
        draw.rectangle([200, 80, 200 + eye_width, 80 + eye_height], fill=color)
        # Bouche triste
        draw.arc([100, 150, 220, 230], start=180, end=360, fill=color, width=15)

    # Sauvegarder strictement au format BMP 24-bits pour le parseur Arduino C++
    print(f"Génération de {filename} ({emotion}) en 320x240...")
    img.save(filename, format='BMP')

if __name__ == "__main__":
    print("🎨 Générateur d'émotions TFT (BMP 24-bits 320x240)")
    create_face("joie.bmp", "joie")
    create_face("neutre.bmp", "neutre")
    create_face("triste.bmp", "triste")
    print("✅ Terminé ! Copiez les fichiers .bmp à la racine de votre carte SD.")
