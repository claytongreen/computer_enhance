#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <random>

#include "../../part2/listing_0065_haversine_formula.cpp"

int main(int argc, char** argv) {
	uint32_t seed;
	uint64_t num_coords;

	if (argc < 3) {
		// TODO: split the string of the program name to just the program name
		printf("Usage: %s <seed> <number of coordinate pairs>\n", argv[0]);
		return 0;
	}

	seed = atoi(argv[1]);
	num_coords = atoi(argv[2]);

	// TODO: generate the things
	std::mt19937 rng(seed);
	std::uniform_real_distribution<f64> dist_x(-180.0, 180.0);
	std::uniform_real_distribution<f64> dist_y(-90.0, 90.0);

	f64 sum = 0;
	
	void *data = malloc((sizeof(f64) * num_coords * 4) + ((sizeof(f64) * num_coords) + 1));
	f64* coords = (f64*)data;
	f64* answers = (f64*)data + (num_coords * 4);
	if (!coords || !answers) return 1;

	f64* ans = answers;
	for (size_t i = 0; i < num_coords; i += 1) {
		f64 x0 = dist_x(rng);
		f64 y0 = dist_y(rng);
		f64 x1 = dist_x(rng);
		f64 y1 = dist_y(rng);

		f64 haversine = ReferenceHaversine(x0, y0, x1, y1, 6372.8);
		sum += haversine;

		*ans++ = haversine;

		coords[i * 4 + 0] = x0;
		coords[i * 4 + 1] = y0;
		coords[i * 4 + 2] = x1;
		coords[i * 4 + 3] = y1;
	}

	sum /= num_coords;
	*ans++ = sum;

	char json_filename[256];
	int length = sprintf(json_filename, "data_%lld.json", num_coords);
	char* haveranswer_filename = json_filename + length + 1;
	sprintf(haveranswer_filename, "data_%lld_haveranswer.f64", num_coords);

	FILE* fp_json = fopen(json_filename, "w+");
	FILE* fp_haveranswer = fopen(haveranswer_filename, "w+b");
	if (fp_json && fp_haveranswer) {
		fprintf(fp_json, "{\n  \"pairs\":[\n");

		f64* coord = coords;
		for (size_t i = 0; i < num_coords - 1; i += 1) {
			fprintf(fp_json, "    {\"x0\":%f, \"y0\":%f, \"x1\":%f, \"y1\":%f},\n", *coord++, *coord++, *coord++, *coord++);
		}
		fprintf(fp_json, "    {\"x0\":%f, \"y0\":%f, \"x1\":%f, \"y1\":%f}\n  ]\n}", *coord++, *coord++, *coord++, *coord++);

		fwrite(answers, sizeof(f64), num_coords + 1, fp_haveranswer);

		fclose(fp_json);
		fclose(fp_haveranswer);
	}
	else {
		if (fp_json) {
			fclose(fp_json);
		}
		else {
			fprintf(stderr, "Failed to open \"%s\"\n", json_filename);
		}
		if (fp_haveranswer) {
			fclose(fp_haveranswer);
		}
		else {
			fprintf(stderr, "Failed to open \"%s\"\n", haveranswer_filename);
		}

		return 1;
	}


	printf("Method: ???\n");
	printf("Random seed: %d\n", seed);
	printf("Pair count: %lld\n", num_coords);
	printf("Expected sum: %f\n", sum);
	printf("\n");
	printf("Saved data to \"%s\"\n", json_filename);
	printf("              \"%s\"\n", haveranswer_filename);

	return 0;
}