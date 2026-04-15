from PIL import Image, ImageDraw

# Résolution stricte de l'écran ILI9341 en mode paysage
WIDTH = 320
HEIGHT = 240

def create_face(filename, emotion, frame=1):
    img = Image.new('RGB', (WIDTH, HEIGHT), color=(0, 0, 0))
    draw = ImageDraw.Draw(img)

    color = (0, 255, 255)

    eye_width, eye_height = 40, 60

    # Gestion du clignement des yeux (Blink) pour la frame 2
    if frame == 2:
        eye_height = 10  # Yeux fermés / plissés
        offset_y = 25
    else:
        offset_y = 0

    left_eye_pos = [80, 60 + offset_y, 80 + eye_width, 60 + eye_height + offset_y]
    right_eye_pos = [200, 60 + offset_y, 200 + eye_width, 60 + eye_height + offset_y]

    if emotion == "neutre":
        draw.rectangle(left_eye_pos, fill=color)
        draw.rectangle(right_eye_pos, fill=color)
        draw.line([100, 180, 220, 180], fill=color, width=10)

    elif emotion == "joie":
        if frame == 2:
            draw.line([70, 70, 130, 70], fill=color, width=15)
            draw.line([190, 70, 250, 70], fill=color, width=15)
        else:
            draw.arc([70, 40, 130, 100], start=180, end=360, fill=color, width=15)
            draw.arc([190, 40, 250, 100], start=180, end=360, fill=color, width=15)
        draw.arc([100, 120, 220, 200], start=0, end=180, fill=color, width=15)

    elif emotion == "triste":
        draw.rectangle(left_eye_pos, fill=color)
        draw.rectangle(right_eye_pos, fill=color)
        draw.arc([100, 150, 220, 230], start=180, end=360, fill=color, width=15)

    elif emotion == "parle":
        # Yeux normaux
        draw.rectangle(left_eye_pos, fill=color)
        draw.rectangle(right_eye_pos, fill=color)
        # Bouche qui s'ouvre et se ferme
        mouth_height = 20 if frame == 1 else 60
        draw.ellipse([120, 160, 200, 160 + mouth_height], fill=color)

    print(f"Génération de {filename} ({emotion} frame {frame}) en 320x240...")
    img.save(filename, format='BMP')

if __name__ == "__main__":
    print("🎨 Générateur d'animations TFT (BMP 24-bits 320x240)")

    # Génération des paires (Yeux ouverts / Yeux fermés)
    emotions = ["joie", "neutre", "triste", "parle"]
    for emo in emotions:
        create_face(f"{emo}_1.bmp", emo, frame=1)
        create_face(f"{emo}_2.bmp", emo, frame=2)

    print("✅ Terminé ! Copiez TOUS les fichiers .bmp à la racine de votre carte SD.")
