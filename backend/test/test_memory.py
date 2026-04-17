import unittest
from unittest.mock import patch, mock_open
import json
import os
import sys

# Add the backend directory to sys.path
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(BASE_DIR)

from src.server import charger_memoire, MEMORY_FILE

class TestMemory(unittest.TestCase):

    @patch('os.path.exists')
    @patch('builtins.open', new_callable=mock_open, read_data=json.dumps({"contexte_utilisateur": "Test context"}))
    def test_charger_memoire_success(self, mock_file, mock_exists):
        mock_exists.return_value = True

        result = charger_memoire()

        self.assertEqual(result, "Test context")
        mock_exists.assert_called_once_with(MEMORY_FILE)
        mock_file.assert_called_once_with(MEMORY_FILE, 'r', encoding='utf-8')

    @patch('os.path.exists')
    def test_charger_memoire_file_not_exists(self, mock_exists):
        mock_exists.return_value = False

        result = charger_memoire()

        self.assertEqual(result, "L'utilisateur est un étudiant/ingénieur travaillant sur un ESP32.")

    @patch('os.path.exists')
    @patch('builtins.open', new_callable=mock_open, read_data=json.dumps({}))
    def test_charger_memoire_missing_key(self, mock_file, mock_exists):
        mock_exists.return_value = True

        result = charger_memoire()

        self.assertEqual(result, "Aucun contexte particulier.")

    @patch('os.path.exists')
    @patch('builtins.open', new_callable=mock_open, read_data="invalid json")
    def test_charger_memoire_invalid_json(self, mock_file, mock_exists):
        mock_exists.return_value = True

        # When json.load fails, it should catch the exception and return the default string
        result = charger_memoire()

        self.assertEqual(result, "L'utilisateur est un étudiant/ingénieur travaillant sur un ESP32.")

if __name__ == '__main__':
    unittest.main()
