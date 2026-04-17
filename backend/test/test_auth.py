import asyncio
import os
import sys
import websockets
# Try to import from the right place depending on version
try:
    from websockets.asyncio.client import connect
except ImportError:
    from websockets import connect

async def test_auth():
    uri = "ws://localhost:8765"
    token = os.environ.get("WS_AUTH_TOKEN", "secure_token_esp32_rayane_2024")

    print("--- Test 1: Connexion SANS token ---")
    try:
        async with connect(uri) as websocket:
            print("❌ Erreur : Connexion acceptée sans token !")
            await websocket.close()
    except Exception as e:
        print(f"✅ Succès : Connexion refusée ({type(e).__name__}: {e})")

    print("\n--- Test 2: Connexion avec un MAUVAIS token ---")
    try:
        headers = {"Authorization": "Bearer wrong_token"}
        # Try both argument names
        try:
            async with connect(uri, additional_headers=headers) as websocket:
                print("❌ Erreur : Connexion acceptée avec un mauvais token !")
                await websocket.close()
        except TypeError:
            async with connect(uri, extra_headers=headers) as websocket:
                print("❌ Erreur : Connexion acceptée avec un mauvais token !")
                await websocket.close()
    except Exception as e:
        print(f"✅ Succès : Connexion refusée ({type(e).__name__}: {e})")

    print("\n--- Test 3: Connexion avec le BON token ---")
    try:
        headers = {"Authorization": f"Bearer {token}"}
        try:
            async with connect(uri, additional_headers=headers) as websocket:
                print("✅ Succès : Connexion acceptée avec le bon token !")
                await websocket.close()
        except TypeError:
            async with connect(uri, extra_headers=headers) as websocket:
                print("✅ Succès : Connexion acceptée avec le bon token !")
                await websocket.close()
    except Exception as e:
        print(f"❌ Erreur : Connexion refusée avec le bon token ({type(e).__name__}: {e})")

if __name__ == "__main__":
    asyncio.run(test_auth())
