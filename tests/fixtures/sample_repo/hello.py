class Animal:
    def __init__(self, name):
        self.name = name

    def speak(self):
        return f"{self.name} speaks"


def greet(name: str) -> str:
    return f"Hello, {name}"
