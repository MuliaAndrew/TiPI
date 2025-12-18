import random
import uuid
import json
from typing import Any, Dict, List, Callable

# Type alias for a function that generates a mock value
GeneratorFunction = Callable[[], Any]

MOCK_WORDS = [
  "alpha", "beta", "gama", "delta", "eps", "zeta", "eta", "thta", "iota", "kapp",
  "lambd", "mu", "nu", "xi", "omcr", "pi", "rho", "sgma", "tau", "ups",
  "phi", "chi", "psi", "omeg", "data", "test", "mock", "temp", "user", "log",
  "app", "sys", "proc", "rec", "idx", "key", "val", "run", "dev", "prod",
  "auth", "sec", "cfg", "meta", "blob", "root", "admin", "main", "core", "node"
]

def _generate_random_email() -> str:
  """Generates a simple random email based on MOCK_WORDS."""
  num_parts = random.randint(2, 7)
  # Ensure word count is less than or equal to the total available words
  num_parts = min(num_parts, len(MOCK_WORDS)) 
  parts = random.sample(MOCK_WORDS, num_parts)
  username = "".join(parts)
  return f"{username}@google.com"

def _mock_datetime() -> str:
  """Generates a random datetime string in YYYY-MM-DDThh:mm:ss format (UTC assumed)."""
  year = 2025
  month = random.randint(1, 12)
  if month in [2]:
    day = random.randint(1, 28)
  elif month in [4, 7, 9, 11]:
    day = random.randint(1, 30)
  else:
    day = random.randint(1, 31)
  hour = random.randint(0, 23)
  minute = random.randint(0, 59)
  second = random.randint(0, 59)
  
  # Format: YYYY-MM-DDThh:mm:ss 
  return f"{year}-{month:02d}-{day:02d}T{hour:02d}:{minute:02d}:{second:02d}"

def _mock_datetime_expanded() -> str:
  _datetime = _mock_datetime()
  micros = random.randint(0, 99999)
  # Format: YYYY-MM-DDThh:mm:ss:micros
  return f"{_datetime}:{micros:05d}"

def extract_schema_from_json_file(path: str) -> Dict[str, Any]:
  """Reads a JSON schema from a file path and returns it as a dictionary."""
  try:
    with open(path, 'r') as f:
      schema = json.load(f)
      return schema
  except FileNotFoundError:
    raise FileNotFoundError(f"Schema file not found at path: {path}")
  except json.JSONDecodeError:
    raise ValueError(f"File at path: {path} contains invalid JSON.")
  except Exception as e:
    raise Exception(f"An unexpected error occurred reading the schema file: {e}")

# --- Core Compilation Logic ---

def compile_schema(schema: Dict[str, Any], depth: int = 0) -> GeneratorFunction:
  """
  Recursively compiles a JSON schema definition into a fast, executable generation function.
  This moves all schema interpretation logic from the generation phase to the configuration phase.
  """

  if depth > 5:
    # Prevent infinite recursion for complex or self-referencing schemas
    return lambda: "MAX_DEPTH_REACHED"

  default_value = schema.get("default")
  enum_values = schema.get("enum")
  schema_type = schema.get("type", "object")

  # 1. Handle fixed values (Enum or Default)
  if enum_values:
    return lambda: random.choice(enum_values)
  
  if default_value is not None:
    return lambda: default_value

  # 2. Handle composite types (Object and Array)
  if schema_type == "object":
    prop_generators: Dict[str, GeneratorFunction] = {}
    required: List[str] = schema.get("required", [])
    
    # Compile generators for all properties
    for prop_name, prop_schema in schema.get("properties", {}).items():
      prop_generators[prop_name] = compile_schema(prop_schema, depth + 1)
    
    optional_keys = [k for k in prop_generators if k not in required]

    # The fast, pre-compiled generation function for an object
    def object_generator() -> Dict[str, Any]:
      result = {}
      keys_to_include = set(required)
      
      # Fast random selection of optional keys (up to 2)
      num_optional = min(len(optional_keys), random.randint(0, 2)) 
      keys_to_include.update(random.sample(optional_keys, num_optional))

      for prop_name in keys_to_include:
        # Call the pre-compiled property generator function
        result[prop_name] = prop_generators[prop_name]()
      return result
    
    return object_generator

  elif schema_type == "array":
    items_schema = schema.get("items", {})
    item_generator: GeneratorFunction = compile_schema(items_schema, depth + 1)
    min_items: int = schema.get("minItems", 1)
    max_items: int = schema.get("maxItems", min_items + 2)

    # The fast, pre-compiled generation function for an array
    def array_generator() -> List[Any]:
      num_items = random.randint(min_items, max_items)
      # Efficient list comprehension calling the item generator repeatedly
      return [item_generator() for _ in range(num_items)]
    
    return array_generator

  # 3. Handle primitive types (String, Number, Boolean, Null)
  elif schema_type == "string":
    format_type = schema.get("format")
    
    if format_type == "uuid":
      return lambda: str(uuid.uuid4())
    elif format_type == "email":
      # Use the new helper function
      return _generate_random_email 
    elif format_type == "date-time":
      # Use the new helper function
      return _mock_datetime if random.randint(0, 1) else _mock_datetime_expanded
    elif format_type == "date":
      return lambda: "2024-05-20"
    elif format_type == "uri":
      return lambda: "https://api.example.com/resource/123"
    else:
      # Fallback string generation
      title = schema.get("title", "string_placeholder").replace(" ", "_").lower()
      return lambda: title

  elif schema_type in ["number", "integer"]:
    minimum = schema.get("minimum", 0)
    maximum = schema.get("maximum", 100)
    
    # Ensure minimum is <= maximum for safety
    if minimum > maximum:
      pass

    if schema_type == "integer":
      # Store bounds and return fast integer generation
      # Python's random.randint handles very large numbers
      return lambda: random.randint(int(minimum), int(maximum))
    else:
      # Store bounds and return fast float generation
      return lambda: round(random.uniform(minimum, maximum), 2)

  elif schema_type == "boolean":
    return lambda: random.choice([True, False])
  
  elif schema_type == "null":
    return lambda: None

  elif isinstance(schema_type, list):
    # Handle 'type' being an array (e.g., ["string", "null"]) - choose one type to compile
    chosen_type = random.choice(schema_type)
    temp_schema = schema.copy()
    temp_schema["type"] = chosen_type
    return compile_schema(temp_schema, depth)

  # Fallback for unhandled types or empty schema
  return lambda: None


class JsonGenerator:
  """
  A configured generator object that holds the compiled generation logic.
  The generate() method is highly optimized for speed.
  """
  def __init__(self, generator_fn: GeneratorFunction):
    self._generator_fn = generator_fn

  def generate(self) -> Dict[str, Any]:
    """
    Generates a new, random JSON object instance based on the pre-compiled schema.
    This method is designed to be called millions of times quickly.
    """
    return self._generator_fn()


def create_json_generator_from_schema(schema: Dict[str, Any]) -> JsonGenerator:
  """
  Factory function to create an optimized JsonGenerator from a JSON schema.
  This function performs the slow, one-time schema compilation.
  """
  compiled_generator = compile_schema(schema)
  return JsonGenerator(compiled_generator)


# --- Example Usage ---

log_event_schema = {
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Log Event",
  "type": "object",
  "properties": {
    "timestamp": {
      "type": "string",
      "format": "date-time",
      "description": "The time the event occurred (YYYY-MM-DDThh:mm:ss format)."
    },
    "user": {
      "type": "string",
      "title": "user_id_001",
      "description": "Identifier of the user."
    },
    "component": {
      "type": "string",
      "title": "data_processor",
      "description": "Component that generated the log event."
    },
    "op": {
      "type": "string",
      "title": "create_record",
      "description": "The operation performed."
    },
    "session_id": {
      "type": "integer",
      "minimum": 0,
      "maximum": 18446744073709551615, # UINT64_MAX
      "description": "Unique session identifier."
    },
    "req_id": {
      "type": "integer",
      "minimum": 0,
      "maximum": 18446744073709551615, # UINT64_MAX
      "description": "Unique request identifier."
    },
    "res": {
      "type": "object",
      "description": "Response dictionary (may be any valid json object).",
      "properties": {
        # For mocking only
        "status": {"type": "string", "enum": ["success", "fail"]},
        "elapsed_ms": {"type": "integer", "minimum": 10, "maximum": 500}
      }
    },
    "attributes": {
      "type": "object",
      "description": "Dictionary of attributes (only strings or integers allowed).",
      "properties": {
        # For mocking only
        "priority": {"type": ["integer", "null"], "enum": [1, 2, 3]},
        "client_ip": {"type": "string", "title": "192.168.1.1"}
      }
    }
  },
  "required": ["timestamp", "user", "op"]
}

# try:
#   generator = create_json_generator_from_schema(log_event_schema)

#   json1 = generator.generate()
#   json2 = generator.generate()

#   print(f'json1: {type(json1)}')
#   print(f'json2: {type(json2)}')

#   print('--- Json 1 --- ')
#   print(json1)
#   print('--- Json 2 --- ')
#   print(json2)
# finally:
#   print('DOne')