"""Project management for UE CLI"""

import json
from pathlib import Path
from typing import Dict, Any, Optional
import uuid
from datetime import datetime


class ProjectManager:
    """Manages UE project sessions"""

    def __init__(self):
        self.current_project = None

    def create_project(self, name: str) -> Dict[str, Any]:
        """Create a new project session"""
        project = {
            'id': str(uuid.uuid4()),
            'name': name,
            'created': datetime.now().isoformat(),
            'modified': False,
            'assets': [],
            'actors': [],
            'materials': [],
            'niagara_systems': []
        }
        self.current_project = project
        return project

    def save_project(self, path: str, project: Optional[Dict[str, Any]] = None):
        """Save project to JSON file"""
        project = project or self.current_project
        if not project:
            raise ValueError("No project to save")

        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)

        with open(path, 'w') as f:
            json.dump(project, f, indent=2)

    def load_project(self, path: str) -> Dict[str, Any]:
        """Load project from JSON file"""
        with open(path, 'r') as f:
            project = json.load(f)

        self.current_project = project
        return project

    def add_asset(self, asset_path: str, asset_type: str):
        """Add asset to project"""
        if not self.current_project:
            raise ValueError("No active project")

        self.current_project['assets'].append({
            'path': asset_path,
            'type': asset_type
        })
        self.current_project['modified'] = True

    def add_actor(self, actor_name: str, actor_type: str):
        """Add actor to project"""
        if not self.current_project:
            raise ValueError("No active project")

        self.current_project['actors'].append({
            'name': actor_name,
            'type': actor_type
        })
        self.current_project['modified'] = True
