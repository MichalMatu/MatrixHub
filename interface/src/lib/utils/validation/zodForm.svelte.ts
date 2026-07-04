import { z } from 'zod';

export interface ZodFormOptions<T extends z.ZodRawShape> {
	schema: z.ZodObject<T>;
	initialValues: z.infer<z.ZodObject<T>>;
	onSubmit: (values: z.infer<z.ZodObject<T>>) => void | Promise<void>;
}

/**
 * A reactive form helper for Svelte 5 using Zod for validation.
 */
export function useZodForm<T extends z.ZodRawShape>(options: ZodFormOptions<T>) {
	type Values = z.infer<z.ZodObject<T>>;

	let values = $state<Values>({ ...options.initialValues });
	let errors = $state<Partial<Record<keyof Values, string>>>({});
	let isSubmitting = $state(false);
	let touched = $state<Partial<Record<keyof Values, boolean>>>({});

	/**
	 * Validates the entire form or a specific field.
	 */
	function validate(field?: keyof Values): boolean {
		const result = options.schema.safeParse(values);

		if (result.success) {
			if (field) {
				delete errors[field];
			} else {
				errors = {};
			}
			return true;
		}

		const formattedErrors = result.error.flatten().fieldErrors as Record<
			string,
			string[] | undefined
		>;

		if (field) {
			const fieldError = formattedErrors[field as string]?.[0];
			if (fieldError) {
				errors[field] = fieldError;
			} else {
				delete errors[field];
			}
		} else {
			const newErrors: Partial<Record<keyof Values, string>> = {};
			for (const key in formattedErrors) {
				newErrors[key as keyof Values] = formattedErrors[key]?.[0];
			}
			errors = newErrors;
		}

		return false;
	}

	/**
	 * Handles field input and optional validation.
	 */
	function handleInput(field: keyof Values, value: Values[keyof Values], validateOnInput = true) {
		values[field] = value;
		if (validateOnInput && touched[field]) {
			validate(field);
		}
	}

	/**
	 * Marks a field as touched and validates it.
	 */
	function handleBlur(field: keyof Values) {
		touched[field] = true;
		validate(field);
	}

	/**
	 * Resets form to initial values.
	 */
	function reset() {
		values = { ...options.initialValues };
		errors = {};
		touched = {};
		isSubmitting = false;
	}

	/**
	 * Submits the form if valid.
	 */
	async function submit() {
		if (isSubmitting) return;

		// Mark all as touched on submit attempt
		const allTouched: Partial<Record<keyof Values, boolean>> = {};
		for (const key in values) {
			allTouched[key as keyof Values] = true;
		}
		touched = allTouched;

		if (!validate()) return;

		isSubmitting = true;
		try {
			await options.onSubmit(values);
		} finally {
			isSubmitting = false;
		}
	}

	return {
		get values() {
			return values;
		},
		get errors() {
			return errors;
		},
		get isSubmitting() {
			return isSubmitting;
		},
		get isValid() {
			return options.schema.safeParse(values).success;
		},
		handleInput,
		handleBlur,
		validate,
		reset,
		submit
	};
}
