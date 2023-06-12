#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(char *cmd);

////////////////////////////////////////////////////////////////////////////////

static double Square(double A) {
  double Result = (A * A);
  return Result;
}

static double RadiansFromDegrees(double Degrees) {
  double Result = 0.01745329251994329577 * Degrees;
  return Result;
}

#define EARTH_RADIUS 6372.8
// NOTE(casey): EarthRadius is generally expected to be 6372.8
static double ReferenceHaversine(double X0, double Y0, double X1, double Y1, double EarthRadius) {
  /* NOTE(casey): This is not meant to be a "good" way to calculate the
     Haversine distance. Instead, it attempts to follow, as closely as possible,
     the formula used in the real-world question on which these homework
     exercises are loosely based.
  */

  double lat1 = Y0;
  double lat2 = Y1;
  double lon1 = X0;
  double lon2 = X1;

  double dLat = RadiansFromDegrees(lat2 - lat1);
  double dLon = RadiansFromDegrees(lon2 - lon1);
  lat1 = RadiansFromDegrees(lat1);
  lat2 = RadiansFromDegrees(lat2);

  double a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2));
  double c = 2.0 * asin(sqrt(a));

  double Result = EarthRadius * c;

  return Result;
}

////////////////////////////////////////////////////////////////////////////////

static double random_in(double min, double max)
{
  double scale = rand() / (double) RAND_MAX;
  double result = min + scale * (max - min);
  return result;
}

static int generate_data(int32_t seed, int32_t num_coord_pairs)
{
  srand(seed);

  // TODO: bufferred write to files

  char *json_filename = (char *)malloc(2048);
  snprintf(json_filename, 1024, "data_%d.json", num_coord_pairs);
  FILE *json = fopen(json_filename, "w+");
  if (!json) {
    fprintf(stderr, "Failed to open file: '%s'\n", json_filename);
    return 1;
  }

  char *haveranswer_filename = json_filename + 1024;
  snprintf(haveranswer_filename, 1024, "data_%d_haveranswer.f64", num_coord_pairs);
  FILE *haveranswer = fopen(haveranswer_filename, "wb+");
  if (!haveranswer) {
    fclose(json);

    fprintf(stderr, "Failed to open file: '%s'\n", haveranswer_filename);
    return 1;
  }

  fprintf(json, "{\n  \"coords\": [\n");

  double avg = 0;
  for (size_t i = 0; i < num_coord_pairs; i += 1) {
    double x0 = random_in(-180.0, 180.0);
    double y0 = random_in( -90.0,  90.0);
    double x1 = random_in(-180.0, 180.0);
    double y1 = random_in( -90.0,  90.0);

    fprintf(json, "    { \"x0\": %.15f, \"y0\": %.15f, \"x1\": %.15f, \"y1\": %.15f }", x0, y0, x1, y1);
    if (i < num_coord_pairs - 1) {
      fprintf(json, ",\n");
    }

    double haversine = ReferenceHaversine(x0, y0, x1, y1, EARTH_RADIUS);
    fwrite(&haversine, sizeof(double), 1, haveranswer);

    avg += haversine;
  }

  avg /= num_coord_pairs;
  fwrite(&avg, sizeof(double), 1, haveranswer);

  fclose(haveranswer);

  fprintf(json, "\n  ]\n}");
  fclose(json);

  printf("Random Seed:      %d\n", seed);
  printf("Coordinate Pairs: %d\n", num_coord_pairs);
  printf("Expected Average: %.15f\n", avg);
  printf("\n");
  printf("  Saved: '%s'\n", json_filename);
  printf("  Saved: '%s'\n", haveranswer_filename);
  printf("\n");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int decode_json(char *filename) {
  // TODO
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  char *command = argv[1];

  if (strcmp(command, "generate") == 0 && argc == 4) {
    int32_t seed = atoi(argv[2]);
    int32_t num_coordinates = atoi(argv[3]);

    return generate_data(seed, num_coordinates);
  } else if (strcmp(command, "decode") == 0 && argc == 3) {
    char *filename = argv[2];

    return decode_json(filename);
  } else {
    print_usage(argv[0]);
    return 1;
  }

  return 0;
}

static void print_usage(char *cmd) {
  printf("Usage: %s [COMMAND]\n", cmd);
  printf("\n");
  printf("COMMAND:\n");
  printf("\tgenerate:  generate json data\n");
  printf("\t%s generate <seed> <num-pairs>\n", cmd);
  printf("\n");
  printf("\tdecode:    decode json data\n");
  printf("\t%s decode <json>\n", cmd);
  printf("\n");
}

