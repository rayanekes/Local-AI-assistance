import sys
import os
import asyncio
import unittest
from unittest.mock import MagicMock, patch, AsyncMock

# Add the root directory to sys.path to allow imports from backend.src
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# Mock missing modules and heavy dependencies to allow importing server.py without installing everything
sys.modules['faster_whisper'] = MagicMock()
sys.modules['silero_vad'] = MagicMock()
sys.modules['llama_cpp'] = MagicMock()
sys.modules['websockets'] = MagicMock()
sys.modules['torch'] = MagicMock()
sys.modules['scipy'] = MagicMock()
sys.modules['scipy.io'] = MagicMock()
sys.modules['scipy.io.wavfile'] = MagicMock()

# Mock heavy dependencies and module-level code to avoid loading models or exiting during import
with patch('os.path.exists', return_value=True), \
     patch('shutil.which', return_value="/usr/bin/piper"):
    from backend.src.server import synthese_memoire_background

class TestMemoryBackground(unittest.IsolatedAsyncioTestCase):

    @patch('backend.src.server.charger_memoire')
    @patch('backend.src.server.sauvegarder_memoire')
    async def test_synthese_memoire_success(self, mock_sauvegarder, mock_charger):
        # Setup mocks
        mock_charger.return_value = "Mémoire initiale"

        mock_llm = MagicMock()
        mock_llm.create_chat_completion.return_value = {
            "choices": [
                {
                    "message": {
                        "content": "Mémoire mise à jour"
                    }
                }
            ]
        }

        # Call the function
        await synthese_memoire_background("Nouvelle info", mock_llm)

        # Assertions
        mock_charger.assert_called_once()
        mock_llm.create_chat_completion.assert_called_once()

        # Check if the prompt contains expected info
        args, kwargs = mock_llm.create_chat_completion.call_args
        messages = kwargs['messages']
        self.assertIn("Mémoire initiale", messages[0]['content'])
        self.assertIn("Nouvelle info", messages[0]['content'])

        # Check if sauvegarder_memoire was called with the correct content
        mock_sauvegarder.assert_called_once_with("Mémoire mise à jour")

    @patch('backend.src.server.charger_memoire')
    @patch('backend.src.server.sauvegarder_memoire')
    async def test_synthese_memoire_llm_failure(self, mock_sauvegarder, mock_charger):
        # Setup mocks
        mock_charger.return_value = "Mémoire initiale"

        mock_llm = MagicMock()
        mock_llm.create_chat_completion.side_effect = Exception("LLM Error")

        # Call the function - it should not raise an exception due to try-except block in code
        try:
            await synthese_memoire_background("Nouvelle info", mock_llm)
        except Exception as e:
            self.fail(f"synthese_memoire_background raised {e} unexpectedly!")

        # Assertions
        mock_charger.assert_called_once()
        mock_llm.create_chat_completion.assert_called_once()
        # sauvegarder_memoire should NOT be called
        mock_sauvegarder.assert_not_called()

    @patch('backend.src.server.charger_memoire')
    @patch('backend.src.server.sauvegarder_memoire')
    async def test_synthese_memoire_empty_response(self, mock_sauvegarder, mock_charger):
        # Setup mocks
        mock_charger.return_value = "Mémoire initiale"

        mock_llm = MagicMock()
        mock_llm.create_chat_completion.return_value = {
            "choices": [
                {
                    "message": {
                        "content": "   "
                    }
                }
            ]
        }

        # Call the function
        await synthese_memoire_background("Nouvelle info", mock_llm)

        # Assertions
        mock_charger.assert_called_once()
        mock_llm.create_chat_completion.assert_called_once()
        mock_sauvegarder.assert_called_once_with("")

if __name__ == '__main__':
    unittest.main()
